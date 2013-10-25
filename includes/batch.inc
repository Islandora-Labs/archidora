<?php

/**
 * @file
 * Batch definition and operation functions.
 */

function archidora_get_archivematica_batch() {
  // TODO: Return an array describing a batch.
}

function archidora_query_objects_operation($jump, &$context) {
  $sandbox = &$context['sandbox'];

  // TODO: Get all objects PIDs, store in a file referenced in
  // $context['results'].

  $content_models_to_skip = array(
    'islandora:collectionCModel',
    'islandora:newspaperCModel',
    'islandora:newspaperIssueCModel',
    'islandora:bookCModel',
  );
  $map = function ($pid) {
    return format_string('"info:fedora/!pid"', array(
      '!pid' => $pid,
    ));
  };

  $qp = new IslandoraSolrQueryProcessor();
  $qp->solrQuery = '*:*';
  $qp->solrParams['fl'] = 'PID';
  $qp->solrParams['fq'] = array(
    '-RELS_EXT_em_iri_uri_ms:[* TO *]',
    format_string('-!field:(!values)', array(
      '!field' => 'RELS_EXT_hasModel_uri_ms',
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

      // TODO: Register all created AIPs somewhere in $context['results']...  In
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

function archidora_finalize_aips(&$context) {
  // TODO: Finalize all AIPs which should be...
}
