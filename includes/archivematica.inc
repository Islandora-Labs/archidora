<?php

/**
 * @file
 * Functions to help with Archivematica operations.
 */

/**
 * Send the given object to Archivematica.
 *
 * @param AbstractObject $object
 *   The object to add.
 * @param object|NULL $aip
 *   NULL to perform a lookup for which AIP to add to, can be provided to
 *   avoid performing the lookup.
 *
 * @return object
 *   The AIP object to which this object has been added, containing the EM-IRI
 *   given back to us from Archivematica.
 */
function archidora_send_to_archivematica(AbstractObject $object, $aip = NULL) {
  if ($aip === NULL) {
    $aip = archidora_lookup_aip($object);
  }
  elseif ($aip === FALSE) {
    // Let's go make a new AIP as this is for an individual object submission.
    module_load_include('inc', 'archidora', 'includes/utilities');
    $parent = archidora_get_parent($object);
    $aip = new stdClass();
    $aip->object = $parent;
    $time = time();
    $randomness = mt_rand(0, pow(2, 20));
    $aip->id = "{$aip->object}--manual--{$time}--{$randomness}";
    $aip->created = TRUE;
    $aip->manual = TRUE;
    $aip->size = 0;
  }
  $atom_file_uri = archidora_get_atom_entry($object);
  // Make request to add to AIP.
  // If $aip->em_iri, POST to it; otherwise, POST to create a new AIP.
  $deposit_location = variable_get('archidora_deposit_location', '');
  if (!$deposit_location) {
    watchdog('archidora', 'Unable to send @id to Archivematica: The deposit location has not been configured.', array(
      '@id' => $object->id,
    ));
    drupal_set_message(t('Unable to send @id to Archivematica: The deposit location has not been configured.', array(
      '@id' => $object->id,
    )), 'warning', FALSE);
    return;
  }
  $url = isset($aip->em_iri) ?
    $aip->em_iri :
    $deposit_location;

  $response = drupal_http_request($url, array(
    'method' => 'POST',
    'headers' => array(
      // Content-Length is automatically determined, based on 'data'.
      'In-Progress' => 'true',
      'Packaging' => 'METS',
    ) + archidora_get_request_base_headers(),
    'data' => file_get_contents($atom_file_uri),
  ));
  file_unmanaged_delete($atom_file_uri);

  if ($response->code == 201 || (isset($aip->manual) && $response->code == 200)) {
    $response_body = $response->data;
    if (!empty($response_body)) {
      $parsed = archidora_parse_deposit_receipt($response_body, $object, $aip->id);
      // Update $aip with stuff parsed out of the deposit receipt.
      if (!isset($aip->se_iri)) {
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
    // Since we are only worried about the OBJ let's go get it now.
    if (isset($object['OBJ'])) {
      $aip->size += $object['OBJ']->size;
    }
    if ($aip->created) {

       if(!isset($aip->size)){$aip->size = 0;}

      db_insert('archidora_aips')
        ->fields(array(
          'id' => $aip->id,
          'created_time' => $aip->created_time,
          'em_iri' => $aip->em_iri,
          'se_iri' => $aip->se_iri,
          'size' => $aip->size,
        ))
        ->execute();
    }
    else {
      db_update('archidora_aips')
        ->fields(array(
          'size' => $aip->size,
        ))
        ->condition('id', $aip->id)
        ->execute();
    }
    if (isset($aip->em_iri)) {
      // Save EM-IRI onto $object, getting rid of any old ones.
      $object->relationships->autoCommit = FALSE;
      $object->relationships->remove(ARCHIDORA_RDF_URI, 'edit_media');
      $object->relationships->add(ARCHIDORA_RDF_URI, 'edit_media', $aip->em_iri, RELS_TYPE_FULL_URI);
      $object->relationships->autoCommit = TRUE;
    }

    module_load_include('inc', 'islandora', 'includes/utilities');
    islandora_invoke_hook_list('archidora_object_sent', $object->models, array(
      $object,
      $aip,
    ));
    if ($aip->manual) {
      archidora_finalize_aip($aip);
    }

    return $aip;
  }
  else {
    // Report error somehow...
    watchdog('archidora', 'Got @code when attempting to add @pid to @aip. Info: @info', array(
      '@code' => $response->code,
      '@pid' => $object->id,
      '@aip' => $aip->id,
      '@info' => print_r($response, TRUE),
    ), WATCHDOG_ERROR);
  }
}

/**
 * Parse out a few values from a Sword deposit receipt into an array.
 *
 * @param string $response_body
 *   A string containing the XML returned as the deposit receipt.
 * @param AbstractObject $object
 *   The relevant object, so we can provide nicer messages...
 * @param string $aip_id
 *   The ID of the AIP we're parsing, so we can report errors nicely.
 *
 * @return array
 *   An associative array containing some useful stuff:
 *   - EM-IRI: The Edit Media IRI.
 *   - SE-IRI: The SE-IRI (to which you submit a blank POST to finialize the
 *     AIP).
 */
function archidora_parse_deposit_receipt($response_body, $object, $aip_id) {
  $to_parse = array(
    'EM-IRI' => '/atom:entry/atom:link[@rel="edit-media"]/@href',
    'SE-IRI' => '/atom:entry/atom:link[@rel="http://purl.org/net/sword/terms/add"]/@href',
  );

  $xml = new SimpleXMLElement($response_body);
  $xml->registerXPathNamespace('atom', 'http://www.w3.org/2005/Atom');

  $to_return = array();
  foreach ($to_parse as $field => $path) {
    $elements = $xml->xpath($path);
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

  return $to_return;
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
  module_load_include('inc', 'archidora', 'includes/utilities');
  $parent = archidora_get_parent($object);

  $q = <<<EOQ
SELECT a.*, s.*
FROM {archidora_aip_base_state} s LEFT JOIN {archidora_aips} a
  ON a.id = s.id
WHERE s.object = :object
EOQ;
  $aip = db_query($q, array(
    ':object' => $parent,
  ))->fetchObject();

  // We found an AIP, but it should be finalized (due to time constraint or
  // whatever)... Get a new one.
  if ($aip && archidora_should_finalize_aip($aip)) {
    // Increment 'part' in {archidora_aip_base_state}.
    archidora_increment_aip_part($aip);

    $aip = db_query($q, array(
      ':object' => $parent,
    ))->fetchObject();
    $aip->created = TRUE;
  }
  elseif ($aip && ($aip->created_time === NULL || $aip->em_iri === NULL || $aip->se_iri === NULL)) {
    // Deal with potential error condition... Entry created in base state table,
    // but request failed before we got to add entry to aips table.
    $aip->created = TRUE;
  }
  elseif (!$aip) {
    // Didn't appear to be any result... Let's fabricate sommat...
    $aip = new stdClass();
    $aip->object = $parent;
    $aip->part = 0;
    $aip->id = "{$aip->object}-{$aip->part}";
    $aip->created = TRUE;
    $aip->size = 0;
    db_merge('archidora_aip_base_state')
      ->key(array('object' => $aip->object))
      ->fields(array(
        'id' => $aip->id,
        'object' => $aip->object,
        'part' => $aip->part,
      ))
      ->execute();
  }
  else {
    $aip->created = FALSE;
  }

  return $aip;
}

/**
 * Determine if we have hit one of the conditions to finalize the given AIP.
 *
 * @param object $aip
 *   AIP object, with whatever properties needed to make a decision about
 *   whether or not it should be finalized, including:
 *   - created_time: The timestamp when this AIP was created.
 *
 * @return bool
 *   TRUE if we should finalize; otherwise, FALSE.
 */
function archidora_should_finalize_aip($aip) {
  $max_age = variable_get('archidora_max_age', 300);
  if ($max_age < 0) {
    // Is a custom time; get it.
    $max_age = intval(variable_get('archidora_max_age_custom', 300));
  }
  $max_size = variable_get('archidora_max_size', 1073741824);
  return (!isset($aip->finalized) || !$aip->finalized) &&
  (isset($aip->created_time) && ((time() - intval($aip->created_time)) > $max_age) ||
  ($aip->size > $max_size));
}

/**
 * Get the base HTTP headers for our SWORD requests.
 *
 * @return array
 *   An array containing HTTP basic auth and Content-Type headers, to use in
 *   drupal_http_request().
 */
function archidora_get_request_base_headers() {
  return array(
    'Authorization' => format_string('Archivematica-API api_key="!api_key", username="!user"', array(
        '!user' => variable_get('archidora_user', 'archidora'),
        '!api_key' => variable_get('archidora_api_key', ''),
    )),
    'Content-Type' => 'application/atom+xml;type=entry',
  );
}

/**
 * Send request to actually finalize an AIP.
 *
 * @param object $aip
 *   Object describing the AIP, with a couple properties:
 *   - se_iri: The SE-IRI, to which we need to make a POST to close the AIP.
 *   - id: The ID of the AIP, so was can report about it.
 */
function archidora_finalize_aip($aip) {
  // Actually send "finalize" post...
  $response = drupal_http_request($aip->se_iri, array(
    'method' => 'POST',
    'headers' => array(
      'In-Progress' => 'false',
    ) + archidora_get_request_base_headers(),
  ));

  if ($response->code == 200) {
    watchdog('archidora', 'Finalized @aip.', array(
      '@aip' => $aip->id,
    ), WATCHDOG_INFO);
    archidora_increment_aip_part($aip);
    $aip->finalized = '1';
    db_update('archidora_aips')
      ->fields(array(
        'finalized' => $aip->finalized,
      ))
      ->condition('id', $aip->id)
      ->execute();
  }
  else {
    watchdog('archidora', 'Failed to finalize AIP @aip. Code: @code Info: @info', array(
      '@aip' => $aip->id,
      '@code' => $response->code,
      '@info' => print_r($response, TRUE),
    ), WATCHDOG_ERROR);
  }
}

/**
 * Send request to delete an AIP.
 *
 * @param object $aip
 *   Object describing the AIP, with a couple properties:
 *   - se_iri: The SE-IRI, to which we need to make a DELETE request to delete
 *     the AIP.
 *   - id: The ID of the AIP, so was can report about it.
 */
function archidora_delete_aip($aip) {
  $response = drupal_http_request($aip->se_iri, array(
    'method' => 'DELETE',
    'headers' => archidora_get_request_base_headers(),
  ));

  if ($response->code == 204) {
    watchdog('archidora', 'Deleted @aip.', array(
      '@aip' => $aip->id,
    ), WATCHDOG_INFO);
  }
  else {
    watchdog('archidora', 'Failed to delete AIP @aip. Code: @code Info: @info', array(
      '@aip' => $aip->id,
      '@code' => $response->code,
      '@info' => print_r($response, TRUE),
    ), WATCHDOG_ERROR);
  }
}

/**
 * Increment the base state, if the given AIP is the most current.
 *
 * @param object $aip
 *   An object describing an AIP (with the "id" property).
 */
function archidora_increment_aip_part($aip) {
  // Increment 'part' in {archidora_aip_base_state}.
  $q = <<<EOQ
UPDATE {archidora_aip_base_state}
SET part = part + 1, id = CONCAT(object, '-', part)
WHERE id = :id
EOQ;
  db_query($q, array(':id' => $aip->id));
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
  $file_uri = file_create_filename("atom.xml", 'public://');

  // Grab the migration METS export and dump in.
  $exported_mets = $object->repository->api->m->export($object->id, array(
    'format' => 'info:fedora/fedora-system:METSFedoraExt-1.1',
    'context' => 'migrate',
    'encoding' => 'UTF-8',
  ));
  file_put_contents($file_uri, $exported_mets);

  return $file_uri;
}

/**
 * Get the EM-IRI from an object (check if it is in an AIP).
 *
 * @param AbstractObject $object
 *   Check if the given object is already in an AIP.
 *
 * @return string|FALSE
 *   The EM-IRI of the AIP in which the $object is contained; otherwise, FALSE.
 */
function archidora_get_em_iri_from_object(AbstractObject $object) {
  $em_iris = $object->relationships->get(ARCHIDORA_RDF_URI, 'edit_media');
  $em_iri = reset($em_iris);
  return $em_iri ? $em_iri['object']['value'] : FALSE;
}

/**
 * Retrieves the se_iri for use in making requests to the API.
 */
function archidora_get_se_iri_from_em_iri($em_iri) {
  $result = db_query('SELECT se_iri from {archidora_aips} WHERE em_iri = :em_iri', array(':em_iri' => $em_iri));
  return $result->fetchColumn();
}


/**
 * Retrieves the status message for an object.
 *
 * @param AbstractObject $object
 *   An AbstractObject representing an object within Fedora.
 *
 * @return string
 *   A string describing the status of the object in Archivematica.
 */
function archidora_get_status_from_object(AbstractObject $object) {
  $to_return = array();
  $em_iri = archidora_get_em_iri_from_object($object);
  // If this object is part of an AIP let's go check the status.
  if ($em_iri) {
    // Parse the em_iri to get the right call for status.
    $se_iri = archidora_get_se_iri_from_em_iri($em_iri);
    $status_endpoint = $se_iri . "state";

    $response = drupal_http_request($status_endpoint, array(
      'method' => 'GET',
      'headers' => archidora_get_request_base_headers(),
    ));
    if ($response->code != 200) {
      $to_return[] = t('Error connecting to Archivematica.');
    }
    else {
      // Parse the status out of the data.
      $data_xml = new SimpleXMLElement($response->data);
      $status = $data_xml->xpath('/atom:feed/atom:category/text()');
      $to_return[] = (string) reset($status);
    }
  }
  else {
    $to_return[] = t('Not currently in Archivematica.');
  }

  module_load_include('inc', 'archidora', 'includes/derivatives');
  if (archidora_is_in_queue($object)) {
    $to_return[] = t('In the queue to be processed.');
  }

  return implode('<br/>', $to_return);
}
