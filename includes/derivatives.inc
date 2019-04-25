<?php

/**
 * @file
 * Functions associated with derivative creation.
 */

/**
 * Derivative callback; send the object to Archivematica.
 *
 *  Function must return an array in the structure of:
 *  - success: Bool denoting whether the operation was successful.
 *     - messages: An array structure containing zero or more array's with the
 *       following fields:
 *       - message: A string passed through t() describing the
 *         outcome of the operation.
 *       - message_sub: (Optional) A substitution array as acceptable by t() or
 *         watchdog.
 *       - type: A string denoting whether the output is to be
 *         drupal_set_messaged (dsm) or watchdogged (watchdog).
 *       - severity: (Optional) A severity level / status to be used when
 *         logging messages. Uses the defaults of drupal_set_message and
 *         watchdog if not defined.
 *
 *
 *
 */
function archidora_derivative_callback($object, $force = FALSE) {
  module_load_include('inc', 'archidora', 'includes/archivematica');
  module_load_include('inc', 'archidora', 'includes/utilities');

    watchdog('archidora_derivatives_callback', 'In derviatives_callback hook @test.', array('@test' => 'a test'), WATCHDOG_WARNING);

  $to_return = array(
      'success' => FALSE,
      'messages' => array(),
  );

  try {
    if (!archidora_is_in_queue($object) &&
      (!archidora_get_em_iri_from_object($object) || !archidora_did_the_last_modification_change_the_em_iri_value($object)) &&
      archidora_should_auto_enqueue($object->id)) {

        watchdog('archidora_derivatives_callback', 'In derviatives_callback hook @test.', array('@test' => 'before encued'), WATCHDOG_WARNING);
      archidora_enqueue_item($object);

        watchdog('archidora_derivatives_callback', 'In derviatives_callback hook @test.', array('@test' => 'object encued'), WATCHDOG_WARNING);

      $to_return['success'] = TRUE;
        $to_return['messages'][] = array(
            'message' => t('@id was automatically enqueued to send to Archivematica'),
            'message_sub' =>  array('@id' => $object->id),
            'type' => 'watchdog',
            'serverity' => 'WATCHDOG_WARNING'
        );
       /*
      $to_return['messages'][] = array('message' => t('Item @id enqueued for archivematica', array('@id' => $object->id)),
                                        'type' => 'dsm',
                                        'serverity' => 'status'

    );
       */
    }
      watchdog('archidora_derivatives_callback', 'In derviatives_callback hook @test.', array('@test' => 'outside try'), WATCHDOG_WARNING);

      $to_return['messages'][] = array(
          'message' => t('@id was not automatically enqueued to send to Archivematica, because we cannot determine if the most recent version of the object'),
          'message_sub' =>  array('@id' => $object->id),
          'type' => 'watchdog',
          'serverity' => 'WATCHDOG_WARNING'
      );

  }
  catch (ArchidoraUnversionedRelExtException $e) {
      $to_return['success'] = FALSE;

      $to_return['messages'][] = array(
          'message' => t('@id was not automatically enqueued to send to Archivematica, because we cannot determine if the most recent version of the object'),
           'message_sub' =>  array('@id' => $object->id),
          'type' => 'watchdog',
          'serverity' => 'WATCHDOG_WARNING'
      );


      watchdog('archidora_derivatives_callback', 'In derviatives_callback hook @test.', array('@test' => 'not to be sent'), WATCHDOG_WARNING);

/*
    watchdog(
      'archidora',
      '%id was not automatically enqueued to send to Archivematica, because we cannot determine if the most recent version of the object has already been sent. Either change the RELS-EXT to be versioned or submit the object manually.',
      array(
        '%id' => $object->id,
      ),
      WATCHDOG_WARNING,
      l(t('object page'), "islandora/object/{$object->id}")
    );
    */
  }

  return $to_return;
}

/**
 * Check if an object is present in our cron queue.
 *
 * @param AbstractObject $object
 *   The object to check.
 *
 * @return bool
 *   TRUE if the object is in the queue; otherwise, FALSE.
 */
function archidora_is_in_queue(AbstractObject $object) {
  $count = db_select('queue', 'q')
    ->fields('q', array())
    ->condition('name', 'archidora_cron_queue')
    ->condition('data', serialize($object->id))
    ->condition('expire', 0)
    ->countQuery()
    ->execute()
    ->fetchField();

  return $count > 0;
}

/**
 * Enqueue an item for processing during cron.
 *
 * @param AbstractObject $object
 *   The object to add to the queue.
 *
 * @return bool
 *   TRUE if the queue is present in the queue after calling; otherwise, FALSE
 *   if we have somehow failed to add to the queue.
 */
function archidora_enqueue_item(AbstractObject $object) {
  $t = db_transaction();

  if (!archidora_is_in_queue($object)) {
    $q = DrupalQueue::get('archidora_cron_queue', TRUE);
    return $q->createItem($object->id);
  }
  return TRUE;
}

/**
 * Check if the last modification changed the EM-IRI, to avoid looping.
 *
 * @param AbstractObject $object
 *   The object to check.
 *
 * @return bool|NULL
 *   TRUE if the last modification changed the EM-IRI, FALSE if it did not, or
 *   NULL if we do not have enough to tell if it changed.
 *
 * @throws ArchidoraUnversionedRelExtException
 *   If the RELS-EXT datastream is not versioned, we cannot reliably determine
 *   if the last modification changed the EM-IRI value of the object.
 */
function archidora_did_the_last_modification_change_the_em_iri_value(AbstractObject $object) {
  if ($object['RELS-EXT']->versionable === FALSE) {
    // We don't know...
    throw new ArchidoraUnversionedRelExtException('Non-versionable RELS-EXT.');
  }
  elseif (count($object['RELS-EXT']) === 1) {
    // There's only one version... can't really say if it changed.
    return NULL;
  }

  $times = array(
    '__OBJECT__' => $object->lastModifiedDate->getTimestamp(),
  );
  foreach ($object as $datastream) {
    $times[$datastream->id] = $datastream->createdDate->getTimestamp();
  }
  $max = max($times);
  $last_modified = array_keys($times, $max);

  if (!in_array('RELS-EXT', $last_modified)) {
    return FALSE;
  }

  module_load_include('inc', 'archidora', 'includes/archivematica');
  $current_em_iri = archidora_get_em_iri_from_object($object);

  $old_relationships = new FedoraRelsExt($object);
  $old_relationships->datastream = $object['RELS-EXT'][1];
  $em_iris = $old_relationships->get(ARCHIDORA_RDF_URI, 'edit_media');
  $em_iri = reset($em_iris);
  $old_em_iri = $em_iri ? $em_iri['object']['value'] : FALSE;

  return $old_em_iri != $current_em_iri;
}

/**
 * Exception class to help with unversioned RELS-EXT.
 */
class ArchidoraUnversionedRelExtException extends Exception {}
