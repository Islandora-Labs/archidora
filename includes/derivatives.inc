<?php

/**
 * @file
 * Functions associated with derivative creation.
 */

/**
 * Derivative callback; send the object to Archivematica.
 */
function archidora_derivative_callback($object, $force = FALSE) {
  module_load_include('inc', 'archidora', 'includes/archivematica');
  if (!archidora_is_in_queue($object) && (!archidora_get_em_iri_from_object($object) || !archidora_did_the_last_modification_change_the_em_iri_value($object))) {
    archidora_enqueue_item($object);
  }
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
 */
function archidora_did_the_last_modification_change_the_em_iri_value(AbstractObject $object) {
  if ($object['RELS-EXT']->versionable === FALSE) {
    // We don't know...
    return NULL;
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
