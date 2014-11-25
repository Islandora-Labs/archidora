<?php
/**
 * @file
 * Object form used in the Archivematica tab and associated functions.
 */

/**
 * Constructs the status tab for use on objects.
 *
 * @param array $form
 *   An array representing a form within Drupal.
 * @param array $form_state
 *   An array containing the Drupal form state.
 * @param AbstractObject $object
 *   An AbstractObject representing an object within Fedora.
 *
 * @return array
 *   The Drupal form to be rendered.
 */
function archidora_object_status_form($form, &$form_state, AbstractObject $object) {
  $collection_cmodels = array(
    'islandora:collectionCModel',
    'islandora:newspaperIssueCModel',
    'islandora:newspaperCModel',
    'islandora:bookCModel',
  );
  $form['object'] = array(
    '#type' => 'hidden',
    '#default_value' => $object->id,
  );
  if (!array_intersect($collection_cmodels, $object->models)) {
    module_load_include('inc', 'archidora', 'includes/archivematica');
    $form['status_fieldset'] = array(
      '#type' => 'fieldset',
      '#title' => t('Status'),
      '#collapsed' => TRUE,
      '#collapsible' => FALSE,
    );
    $form['status_fieldset']['status'] = array(
      '#type' => 'markup',
      '#markup' => archidora_get_status_from_object($object),
    );
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Send to Archivematica'),
    );
  }
  else {
    module_load_include('inc', 'archidora', 'includes/utilities');
    // Go get all the objects that have been flagged for deletion in this
    // collection.
    $objects_query = <<<EOQ
SELECT ?object ?label FROM <#ri> WHERE {
  {
  ?object <fedora-rels-ext:isMemberOf> <info:fedora/$object->id>
  }
  UNION
  {
  ?object <fedora-rels-ext:isMemberOfCollection> <info:fedora/$object->id>
  }
  ?object <fedora-view:disseminates> [<http://islandora.ca/archivematica#isReadyForDeletion> 'TRUE'] ;
          <fedora-model:label> ?label
}
EOQ;
    $results = $object->repository->ri->sparqlQuery($objects_query);
    $rows = array();
    foreach ($results as $result) {
      $rows[$result['object']['value']] = array(
        l(filter_xss($result['label']['value']), "islandora/object/{$result['object']['value']}"),
        l(t('delete'), "islandora/object/{$result['object']['value']}/datastream/OBJ/delete"),
      );
    }
    $form['object_deletion_table'] = array(
      '#theme' => 'table',
      '#rows' => $rows,
      '#header' => array(
        t('Object'),
        t('Operations'),
      ),
      '#empty' => t('No OBJs currently awaiting deletion.'),
      '#caption' => t('Objects with OBJs awaiting deletion'),
    );
    $form['disable_auto_enqueument'] = array(
      '#type' => 'checkbox',
      '#title' => t("Don't Archive Children"),
      '#description' => t("Don't enqueue children on ingest to be sent to archivematica."),
      '#default_value' => archidora_auto_enqueue_disabled($object),
    );
    $form['#submit'] = array('archidora_aggregate_object_status_form_submit');
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save Settings'),
    );
  }
  return $form;
}

/**
 * Submit handler for the Archivematica object status form.
 *
 * @param array $form
 *   An array representing a form within Drupal.
 * @param array $form_state
 *   An array containing the Drupal form state.
 */
function archidora_object_status_form_submit($form, &$form_state) {
  module_load_include('inc', 'archidora', 'includes/archivematica');
  $object = islandora_object_load($form_state['values']['object']);
  $success = archidora_send_to_archivematica($object, FALSE);
  if ($success === NULL) {
    drupal_set_message(t('Unable to submit to Archivematica. Check the watchdog logs for more information.'), 'error');
  }
  else {
    drupal_set_message(t('Successfully submitted to Archivematica.'));
  }
}

/**
 * Submit handler for the archivematica aggregate object status form.
 */
function archidora_aggregate_object_status_form_submit($form, &$form_state) {
  module_load_include('inc', 'archidora', 'includes/utilities');
  $object = islandora_object_load($form_state['values']['object']);
  if ($form_state['values']['disable_auto_enqueument'] != archidora_auto_enqueue_disabled($object)) {
    archidora_set_auto_enqueue_disabled(
      $object,
      $form_state['values']['disable_auto_enqueument']
    );
  }
}
