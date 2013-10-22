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

  // TODO: Make request to add to AIP.
  // TODO: If $aip->em_iri, POST to it; otherwise, POST to create a new AIP.
  // TODO: Parse response; adjust size stored in table if successful.
  // TODO: Save EM-IRI onto $object, and into $aip/table.
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
SELECT *, CONCAT(object, '-', part) AS aip_id
FROM {archidora_aip_info} INNER JOIN (
  SELECT MAX(part) p
  FROM {archidora_aip_info}
  WHERE object = :object AND finalized_date IS NULL
) i ON part = i.part
WHERE object = :object
EOQ;
  $results = db_query($q, array(
    ':object' => $parent->id,
  ))->fetchAll();
  $aip = reset($results);

  if (!$aip) {
    // Didn't appear to be any result... Let's fabricate sommat...
    $aip = new stdClass();
    $aip->object = $parent->id;
    $aip->part = 0;
    $aip->aip_id = "{$aip->object}-{$aip->part}";
  }

  return $aip;
}

/**
 * Determine if we have hit one of the conditions to finalize the given AIP.
 *
 * @param object $aip_id
 *
 * @return bool
 *   TRUE if we should finalize; otherwise, FALSE.
 */
function archidora_should_finalize_aip($aip) {
  // TODO: Return `AIP size > threshold || date > expiry`?
  return FALSE;
}

function archidora_finalize_aip($aip_id) {
  // TODO: Set expiry this AIP?
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
  // TODO: Build up an ATOM entry for the given object...

  $atom_ns = 'http://www.w3.org/2005/Atom';

  $file_uri = file_create_filename("atom.xml", 'public://');
  $xml = new XMLWriter();
  $xml->openURI($file_uri);
  $xml->startDocument('1.0', 'UTF-8');
  $xml->startElementNS('atom', 'entry', $atom_ns);

  $xml->startElementNS('atom', 'title', $atom_ns);
  $xml->text($object->label);
  $xml->endElement();

  $xml->startElementNS('atom', 'id', $atom_ns);
  $xml->text($object->id);
  $xml->endElement();

  $xml->startElementNS('atom', 'updated', $atom_ns);
  $xml->text($object->lastModifiedDate->toString())
  $xml->endElement();

  $xml->startElementNS('atom', 'author', $atom_ns);
  $xml->startElementNS('atom', 'name', $atom_ns);
  $xml->text($object->owner);
  $xml->endElement();
  $xml->endElement();

  // Grab the migration METS export and dump in.
  $exported_mets = $object->repository->api->m->export($object->id, array(
    'format' => 'info:fedora/fedora-system:METS-1.1',
    'context' => 'migrate',
  ));
  $xml->writeRaw($exported_mets);

  $xml->endElement();
  $xml->endDocument();
  $xml->flush();
  return $file_uri;
}
