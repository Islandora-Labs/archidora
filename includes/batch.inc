<?php

/**
 * @file
 * Batch definition and operation functions.
 */

/**
 * Get a batch definition to process...
 *
 * Not actually sure how useful this might be, on second though, with the way
 * we add things in the derivative hook... Shrug?
 *
 * @return array
 *   An array structure which should be able to be passed to batch_set().
 */
function archidora_get_archivematica_batch() {
  module_load_include('inc', 'archidora', 'includes/utilities');

  // Return an array describing a batch.
  return array(
    'title' => t('Sending objects to Archivematica...'),
    'init_message' => t('Prepping...'),
    'progress_message' => t('Time elapsed: @elapsed <br/>Estimated time remaning @estimate.'),
    'operations' => array(
      array(
        'archidora_query_objects_operation',
        array(
          1000,
          archidora_skip_content_models(),
        ),
      ),
      array('archidora_process_object_operation', array()),
      array('archidora_finalize_aips', array()),
    ),
    'file' => drupal_get_path('module', 'archidora') . '/includes/batch.inc',
  );
}

/**
 * Batch operation to discover all objects on which we have to operate.
 *
 * Get all objects PIDs, store in a file referenced in $context['results'].
 *
 * @param int $jump
 *   The number of items for which to query at a time.
 * @param array $content_models_to_skip
 *   An array of string representing content models, as provided by
 *   archidora_skip_content_models().
 */
function archidora_query_objects_operation($jump, $content_models_to_skip, &$context) {
  $sandbox = &$context['sandbox'];

  $map = function ($pid) {
    return format_string('"info:fedora/!pid"', array(
      '!pid' => $pid,
    ));
  };

  $qp = new IslandoraSolrQueryProcessor();
  $qp->solrQuery = '*:*';
  $qp->solrParams['fl'] = 'PID';
  $qp->solrParams['fq'] = array(
    format_string('-!field:[* TO *]', array(
      '!field' => variable_get('archidora_em_iri_solr_field', 'RELS_EXT_edit_media_uri_ms'),
    )),
    format_string('-!field:(!values)', array(
      '!field' => variable_get('islandora_solr_content_model_field', 'RELS_EXT_hasModel_uri_ms'),
      '!values' => implode(' ', array_map($map, $content_models_to_skip)),
    )),
  );

  if (!isset($sandbox['offset'])) {
    $sandbox['offset'] = 0;
    $context['results']['pids_in_file'] = 0;
    $context['results']['file'] = drupal_tempnam('public://', 'archidora_batch');
    touch($context['results']['file']);
  }
  else {
    $sandbox['offset'] += $jump;
  }

  $qp->solrStart = $sandbox['offset'];
  $qp->solrLimit = $jump;

  $qp->executeQuery(FALSE, TRUE);
  $sandbox['total'] = $qp->islandoraSolrResult['response']['numFound'];

  if (!($sandbox['total'] > 0)) {
    $context['message'] = t('Nothing found.');
    return;
  }

  $file = fopen($context['results']['file'], 'ab');
  $context['results']['pids_in_file'] += count($qp->islandoraSolrResult['response']['objects']);
  foreach ($qp->islandoraSolrResult['response']['objects'] as $result) {
    fwrite($file, "{$result['solr_doc']['PID']}\n");
  }
  fflush($file);
  fclose($file);

  $context['finished'] = count($qp->islandoraSolrResult['response']['objects']) === 0;
}

/**
 * Batch operation to process all discovered operands.
 */
function archidora_process_object_operation(&$context) {
  $sandbox = &$context['sandbox'];

  if (!isset($sandbox['file_offset'])) {
    $sandbox['file_offset'] = 0;
    $sandbox['total'] = $context['results']['pids_in_file'];
    $sandbox['offset'] = 0;
  }

  if (!($sandbox['total'] > 0)) {
    return;
  }

  $file = fopen($context['results']['file'], 'r');
  fseek($file, $sandbox['file_offset']);
  $pid = trim(fgets($file));
  if (!empty($pid)) {
    $object = islandora_object_load($pid);
    // Send to Archivematica... The called function should test if the
    // (current version of the) object is already in Archivematica... likely
    // looking at relationships?
    if ($object) {
      module_load_include('inc', 'archidora', 'includes/archivematica');
      $aip = archidora_send_to_archivematica($object);

      // Register all created AIPs somewhere in $context['results']...  In
      // an associative array, mapping IDs to whether or not it should be
      // finalized... Will need the SE-IRI to be able to finalize... Maybe a
      // more detailed structure?
      $context['results']['aips'][$aip->id] = $aip;
    }
  }
  $sandbox['file_offset'] = ftell($file);
  $sandbox['offset']++;
  fclose($file);

  $context['finished'] = $sandbox['offset'] / $sandbox['total'];
}

/**
 * Batch operation to finalize AIPs encountered when batching...
 */
function archidora_finalize_aips(&$context) {
  $sandbox = &$context['sandbox'];
  // TODO: Finalize all AIPs which should be...
  if (!isset($sandbox['offset'])) {
    $sandbox['aips'] = array_values($context['results']['aips']);
    $sandbox['total'] = count($sandbox['aips']);
    if ($sandbox['total'] === 0) {
      return 0;
    }
  }

  $aip = array_pop($sandbox['aips']);

  module_load_include('inc', 'archidora', 'includes/archivematica');
  if (archidora_should_finalize_aip($aip)) {
    archidora_finalize_aip($aip);
  }
  $context['finished'] = ($sandbox['total'] - count($sandbox['aips'])) / $sandbox['total'];
}
