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
  module_load_include('inc', 'archidora', 'includes/utilities');
  if (!archidora_is_in_queue($object) && archidora_should_auto_enqueue($object->id)) {
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
