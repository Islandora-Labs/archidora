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
  if (!in_array('islandora:collectionCModel', $object->models)) {
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
  $object = menu_get_object('islandora_object', 2);
  $success = archidora_send_to_archivematica($object);
  if ($success === NULL) {
    drupal_set_message(t('Unable to submit to Archivematica. Check the watchdog logs for more information.'), 'error');
  }
  else {
    drupal_set_message(t('Successfully submitted to Archivematica.'));
  }
}
