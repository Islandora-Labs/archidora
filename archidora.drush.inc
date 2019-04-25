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
      'drush -u 1 archidora-send-collection-to-archivematica --target=pid --children=(yes,no)',
    ),
    'options' => array(
      'target' => array(
        'description' => 'The root pid of where to begin our search.',
        'required' => TRUE,
      ),
      'children' => array(
        'description' => 'Send child collections to archivematica (yes or no)',
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
  $children = drush_get_option('children');
  batch_set(archidora_send_collection_create_batch($target, $children));
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
function archidora_send_collection_create_batch($target, $children) {
  return array(
    'operations' => array(
      array('archidora_send_collection_batch_operation', array($target, $children)),
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
function archidora_send_collection_batch_operation($target, $children, &$context) {

    $logpath = drupal_get_path('module', 'archidora') . '/drush.log';

  file_put_contents($logpath, "Starting new batch: " . $target ." with children set to " . $children . "\n");

  //Select object that are children and get count
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

  //Append to the query with offset so the set is not to large
  $offset_start = $sandbox['offset'];
  $query .= "
  LIMIT $object_update
  OFFSET $offset_start
  ";
  $results = $connection->repository->ri->sparqlQuery($query);

  //Define the collections that need to be recursed into
                       
  $collections = array('islandora:collectionCModel', 'islandora:bookCModel');
  $pidArr = array(); 

/*
  This section of code checks to see if the object is a collection and if we want to 
  capture children. It collects pids into an array as required.  The array is then
  processed.
*/
  foreach ($results as $result) {
    module_load_include('inc', 'archidora', 'includes/archivematica');
    $object = islandora_object_load($result['pid']['value']);

    //This was added for recursion
    //get relationships
    $relationships = $object->relationships->get('info:fedora/fedora-system:def/model#', 'hasModel');
    //test for collection
    if(in_array($relationships[0]['object']['value'], $collections) && strcasecmp($children, 'yes') == 0){
       //Debugin 
       file_put_contents($logpath, "Found collection in top function: " . $result['pid']['value'] ." processing children\n", FILE_APPEND);
       getSubCollectionPids($result['pid']['value'], $pidArr);
    }else{
       array_push($pidArr, $result['pid']['value']);
       $curPid = $result['pid']['value'];
       file_put_contents($logpath, "Adding: " . $curPid ."\n", FILE_APPEND);
       //archidora_send_to_archivematica($object);
    }
  }

  $context['message'] = t('Processing @end results.', array(
    /*'@start' => $sandbox['offset'],*/
    '@end' => count($pidArr),
  ));

  foreach($pidArr as $k => $v){

    $object = islandora_object_load($v);
    if(isset($object->id)) {
        archidora_send_to_archivematica($object);
    }
    else {
          drupal_set_message(t('Error retrieving object %s', array('%s' => $v), 'error', FALSE));
    }
  }
  $sandbox['offset'] += $object_update;
  $context['finished'] = $sandbox['offset'] / $sandbox['total'];
}


function getSubCollectionPids($target, &$pidArr){

    $logpath = drupal_get_path('module', 'archidora') . '/drush.log';

  Global $repository;
  $collections = array('islandora:collectionCModel', 'islandora:bookCModel');

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

  $sparql_count = $connection->repository->ri->countQuery($query, 'sparql');
  $results = $connection->repository->ri->sparqlQuery($query);

  foreach ($results as $result) {
    $fedoraObject = islandora_object_load($result['pid']['value']);
    $relationships = $fedoraObject->relationships->get('info:fedora/fedora-system:def/model#', 'hasModel');
    if(in_array($relationships[0]['object']['value'], $collections)){
       //This is where you need to recurse to get the rest.
       file_put_contents($logpath, "Processing child collection: " . $result['pid']['value'] ."\n", FILE_APPEND);
       getSubCollectionPids($result['pid']['value'], $pidArr);
    }else{
	//Here you add to array	
       file_put_contents($logpath, "Adding child object: " . $result['pid']['value'] ."\n", FILE_APPEND);
	array_push($pidArr, $result['pid']['value']);
    }
  }
}






