<?php
/**
 * @file
 * Sample Drush script to iterate through a collection.
 */

/**
 * Implemenets hook_drush_command().
 */
function archidora_drush_command() {
  $commands = array();
  $commands['archidora-send-collection-to-archivematica'] = array(
    'aliases' => array('asca'),
    'description' => dt('Sends the members of a collection to Archivematica. Note: This only works on a flat level and is only intended as an example. Use at your own risk.'),
    'drupal dependencies' => array(
      'islandora',
      'archidora',
    ),
    'examples' => array(
      'drush -u 1 archidora-send-collection-to-archivematica',
    ),
    'options' => array(
      'target' => array(
        'description' => 'The root pid of where to begin our search.',
        'required' => TRUE,
      ),
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
  );
  return $commands;
}

/**
 * Sends a collection of objects to Archivematica and then finalizes the AIP.
 */
function drush_archidora_send_collection_to_archivematica() {
  $target = drush_get_option('target');
  batch_set(archidora_send_collection_create_batch($target));
  drush_backend_batch_process();
}

/**
 * Creates a sample batch to go and add all members of a collection.
 *
 * @param string $target
 *   The root pid to start our RI query from.
 *
 * @return array
 *   An array describing the batch to be ran.
 */
function archidora_send_collection_create_batch($target) {
  return array(
    'operations' => array(
      array('archidora_send_collection_batch_operation', array($target)),
    ),
    'title' => t('Sending the collection to Archivematica...'),
    'init_message' => t('Preparing to send the collection.'),
    'progress_message' => t('Time elapsed: @elapsed <br/>Estimated time remaining @estimate.'),
    'error_message' => t('An error has occurred.'),
    'file' => drupal_get_path('module', 'archidora') . '/archidora.drush.inc',
  );
}

/**
 * Constructs and performs the send collection operation.
 *
 * @param string $target
 *   The root pid to start our RI query from.
 *
 * @param array $context
 *   The context of the Drupal batch.
 */
function archidora_send_collection_batch_operation($target, &$context) {
  $object_update = 10;
  $query = <<<EOQ
SELECT ?pid FROM <#ri>
WHERE {
    {
  ?pid <fedora-rels-ext:isMemberOf> <info:fedora/$target>
  }
  UNION
  {
  ?pid <fedora-rels-ext:isMemberOfCollection> <info:fedora/$target>
  }
}
EOQ;
  $connection = islandora_get_tuque_connection();

  $sandbox = &$context['sandbox'];
  if (!isset($sandbox['offset'])) {
    $sparql_count = $connection->repository->ri->countQuery($query, 'sparql');
    $sandbox['offset'] = 0;
    $sandbox['total'] = $sparql_count;
    if ($sandbox['total'] === 0) {
      return;
    }
  }

  $context['message'] = t('Processing results @start to @end.', array(
    '@start' => $sandbox['offset'],
    '@end' => min($sandbox['offset'] + $object_update, $sandbox['total']),
  ));

  $offset_start = $sandbox['offset'];
  $query .= "
  LIMIT $object_update
  OFFSET $offset_start
  ";
  $results = $connection->repository->ri->sparqlQuery($query);
  foreach ($results as $result) {
    module_load_include('inc', 'archidora', 'includes/archivematica');
    $object = islandora_object_load($result['pid']['value']);
    archidora_send_to_archivematica($object);
  }
  $sandbox['offset'] += $object_update;
  $context['finished'] = $sandbox['offset'] / $sandbox['total'];
}
