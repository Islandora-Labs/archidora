<?php
/**
 * @file
 * A batch for purging datastreams.
 */

/**
 * Defines a batch for purging datastreams.
 *
 * @param array|string $to_purge
 *   As array('pid') or a PID for all children.
 */
function archidora_obj_purge_batch($to_purge) {
  $operations = array();
  if (is_array($to_purge)) {
    foreach ($to_purge as $pid) {
      $operations[] = array(
        'archidora_purge_obj_batch_op',
        array($pid),
      );
    }
  }
  else {
    $operations[] = array(
      'archidora_purge_all_objs_batch_op',
      array($to_purge),
    );
  }

  return array(
    'title' => t('Purging Datastreams...'),
    'init_message' => t('Preparing to purge datastreams...'),
    'progress_message' => t('Time elapsed: @elapsed <br/>Estimated time remaining: @estimate.'),
    'operations' => $operations,
    'file' => drupal_get_path('module', 'archidora') . '/includes/obj_purge.batch.inc',
  );
}

/**
 * A batch operation for purging datastreams.
 *
 * @param string $aggregate
 *   The PID of the object to purge its childrens datastream from.
 */
function archidora_purge_all_objs_batch_op($aggregate, &$context) {
  module_load_include('inc', 'archidora', 'includes/utilities');
  $context['finished'] = 0;
  $results = archidora_flagged_objs($aggregate, 1, 0);
  // Bail out if nothing meets the query.
  if (!$results) {
    $context['finished'] = 1;
    return;
  }
  $result = reset($results);
  archidora_purge_datastream($result['object']['value'], 'OBJ');
}

/**
 * A batch operation for purging datastreams.
 *
 * @param string $pid
 *   The PID of the object to purge the datastream from.
 */
function archidora_purge_obj_batch_op($pid, &$context) {
  module_load_include('inc', 'archidora', 'includes/utilities');
  archidora_purge_datastream($pid, 'OBJ');
}
