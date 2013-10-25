<?php

/**
 * @file
 * Functions to help with Archivematica operations.
 */

/**
 *
 *
 * @param AbstractObject $object
 *   The object to add.
 * @param object|NULL $aip
 *   NULL to perform a lookup for which AIP to add to, can be provided to
 *   avoid performing the lookup.
 *
 * @return string
 *   The EM-IRI given back to us from Archivematica.
 */
function archidora_send_to_archivematica(AbstractObject $object, $aip = NULL) {
  if ($aip === NULL) {
    $aip = archidora_lookup_aip($object);
  }

  $atom_file_uri = archidora_get_atom_entry($object);
  // Make request to add to AIP.
  // If $aip->em_iri, POST to it; otherwise, POST to create a new AIP.
  $archivematica_url = variable_get('archidora_archivematica_url', 'http://localhost');
  $url = isset($aip->em_iri) ?
    $aip->em_iri :
    "$archivematica_url/api/v2/transfer/create";

  $response = drupal_http_request($url, array(
    'method' => 'POST',
    'headers' => array(
      // Content-Length is automatically determined, based on 'data'.
      'In-Progress' => 'true',
      // TODO: Figure out what to pass for auth...
      //'Authorization' => format_string('Basic !b64d', array('!b64d' => )),
      'Content-Type' => 'application/atom+xml;type=entry',
      'Slug' => $aip->id,
    ),
    'data' => rawurlencode(file_get_contents($atom_file_uri)),
  ));
  file_unmanaged_delete($atom_file_uri);

  if ($response->code === 201) {
    // TODO: Adjust size stored in table when successful, for AM-4?
    $response_body = $response->data;
    if (!empty($response_body)) {
      $parsed = archidora_parse_deposit_receipt($response_body, $object, $aip->id);
      // TODO: Update $aip with stuff parsed out of the deposit receipt.
      if (!isset($aip->se_iri)) {
        $aip->created = TRUE;
        $aip->created_time = time();
        $aip->se_iri = $parsed['SE-IRI'];
        $aip->em_iri = $parsed['EM-IRI'];
      }
    }
    else {
      watchdog('archidora', 'No deposit receipt received when adding @pid to @aip.', array(
        '@pid' => $object->id,
        '@aip' => $aip->id,
      ), WATCHDOG_ERROR);
    }

    if (isset($aip->em_iri)) {
      // TODO: Save EM-IRI onto $object.
    }
    return $aip;
  }
  else {
    // Report error somehow...
    watchdog('archidora', 'Got @code when attempting to add @pid to @aip.', array(
      '@code' => $response->code,
      '@pid' => $object->id,
      '@aip' => $aip->id,
    ), WATCHDOG_ERROR);
  }
}

function archidora_parse_deposit_receipt($response_body, $object, $aip_id) {
  $to_parse = array(
    'EM-IRI' => '/atom:entry/atom:link[@rel="edit-media"]/@href',
    'SE-IRI' => '/atom:entry/atom:link[@rel="http://purl.org/net/sword/terms/add"]/@href',
  );

  $xml = new SimpleXMLElement($response_body);
  $xml->registerXPathNamespace('atom', 'http://www.w3.org/2005/Atom');

  $to_return = array();
  foreach ($to_parse as $field => $path) {
    $elements = $xml->xpath('/atom:entry/atom:link[@rel="edit-media"]/@href');
    $element = reset($elements);
    if ($element) {
      $to_return[$field] = (string) $element;
    }
    else {
      watchdog('archidora', 'Deposit receipt did not contain @field when adding @pid to @aip.', array(
        '@field' => $field,
        '@pid' => $object->id,
        '@aip' => $aip_id,
      ), WATCHDOG_WARNING);
    }
  }
}

/**
 * Get an AIP ID for the current object.
 *
 * Grab the collection/parent as necessary.
 *
 * @param AbstractObject $object
 *   The object for which to get the AIP to which to add it.
 *
 * @return object
 *   Either the AIP currently in progress, or a new AIP.
 */
function archidora_lookup_aip(AbstractObject $object) {
  module_load_include('inc', 'archidora', 'utilities');
  $parent = archidora_get_parent($object);

  $q = <<<EOQ
SELECT *, CONCAT(object, '-', part) AS id
FROM {archidora_aip_info}
WHERE object = :object
EOQ;
  $aip = db_query($q, array(
    ':object' => $parent->id,
  ))->fetchObject();

  if (!$aip) {
    // Didn't appear to be any result... Let's fabricate sommat...
    $aip = new stdClass();
    $aip->object = $parent->id;
    $aip->part = 0;
    $aip->id = "{$aip->object}-{$aip->part}";
  }

  return $aip;
}

/**
 * Determine if we have hit one of the conditions to finalize the given AIP.
 *
 * @param object $aip
 *
 * @return bool
 *   TRUE if we should finalize; otherwise, FALSE.
 */
function archidora_should_finalize_aip($aip) {
  // TODO: Return `AIP size > threshold || date > expiry`?
  return FALSE;
}

function archidora_finalize_aip($aip) {
  // TODO: Increment 'part' in archidora_aip_info.
}

/**
 * Get ATOM entry to throw at Archivematica.
 *
 * @param AbstractObject $object
 *   The object for which to build the entry.
 *
 * @return string
 *   A generated file URI containing the ATOM entry. Delete it when you're done
 *   with it.
 */
function archidora_get_atom_entry(AbstractObject $object) {
  // Build up an ATOM entry for the given object...
  $atom_ns = 'http://www.w3.org/2005/Atom';

  $file_uri = file_create_filename("atom.xml", 'public://');
  $xml = new XMLWriter();
  $xml->openURI($file_uri);
  $xml->startDocument('1.0', 'UTF-8');
  $xml->startElementNS('atom', 'entry', $atom_ns);

  $xml->startElementNS('atom', 'title', NULL);
  $xml->text($object->label);
  $xml->endElement();

  $xml->startElementNS('atom', 'id', NULL);
  $xml->text($object->id);
  $xml->endElement();

  $xml->startElementNS('atom', 'updated', NULL);
  $xml->text($object->lastModifiedDate->format(DateTime::ATOM));
  $xml->endElement();

  $xml->startElementNS('atom', 'author', NULL);
  $xml->startElementNS('atom', 'name', NULL);
  $xml->text($object->owner);
  $xml->endElement();
  $xml->endElement();

  // Grab the migration METS export and dump in.
  $exported_mets = $object->repository->api->m->export($object->id, array(
    'format' => 'info:fedora/fedora-system:METSFedoraExt-1.1',
    'context' => 'migrate',
    'encoding' => 'UTF-8',
  ));
  // XXX: Strip off the "<?xml version, encoding..." bit.
  if (strpos($exported_mets, '<?xml') === 0) {
    $dom = new DOMDocument();
    $dom->preserveWhiteSpace = FALSE;
    $dom->loadXML($exported_mets);
    $exported_mets = $dom->saveXML($dom->documentElement);
  }
  $xml->writeRaw($exported_mets);

  $xml->endElement();
  $xml->endDocument();
  $xml->flush();
  return $file_uri;
}
