<?php

/**
 * @file
 * Admin form and associated functions.
 */

/**
 * Form building function.
 */
function archidora_admin_form($form, &$form_state) {
  form_load_include($form_state, 'inc', 'archidora', 'includes/admin.form');

  $form['archivematica'] = array(
    '#type' => 'fieldset',
    '#title' => t('Archivematica info'),
  );
  // XXX: Would be nice to be able to verify the provided details, here;
  // however, there doesn't appear to be an endpoint we can hit to test...
  $form['archivematica']['archidora_archivematica_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Archivematica Base URL'),
    '#description' => t('The base URL to use to contact Archivematica.'),
    '#default_value' => variable_get('archidora_archivematica_url', 'http://localhost'),
  );
  $form['archivematica']['archidora_user'] = array(
    '#type' => 'textfield',
    '#title' => t('Archivematica User'),
    '#description' => t('The username to use to connect to Archivematica.'),
    '#required' => TRUE,
    '#default_value' => variable_get('archidora_user', 'archidora'),
  );
  $form['archivematica']['archidora_api_key'] = array(
    '#type' => 'textfield',
    '#title' => t('Archivematica API Key'),
    '#description' => t('An API key for the user indicated above.'),
    '#required' => TRUE,
    '#default_value' => variable_get('archidora_api_key', ''),
  );
  $form['archidora_em_iri_solr_field'] = array(
    '#type' => 'textfield',
    '#title' => t('EM-IRI Solr field'),
    '#description' => t('A Solr field which will contain the EM-IRI.'),
    '#default_value' => variable_get('archidora_em_iri_solr_field', 'RELS_EXT_edit_media_uri_ms'),
  );

  $age_options = array(
    0,
    300,
    600,
    1800,
    3600,
    6 * 3600,
    12 * 3600,
    86400,
    604800,
    2592000,
    3 * 2592000,
    6 * 2592000,
    31536000,
  );
  $age_options = array(
    -1 => t('Custom'),
  ) + array_map('format_interval', drupal_map_assoc($age_options));
  $form['archidora_max_age'] = array(
    '#type' => 'select',
    '#title' => t('AIP Max Age'),
    '#description' => t('New objects will not be added to an AIP after it reaches this age, and it will be flagged for finalization at the next run.'),
    '#options' => $age_options,
    '#default_value' => variable_get('archidora_max_age', 300),
  );
  $form['archidora_max_age_custom'] = array(
    '#type' => 'textfield',
    '#title' => t('Custom max age'),
    '#description' => t('An interval, expressed as seconds.'),
    '#element_validate' => array(
      'archidora_custom_max_age_provided',
      'element_validate_integer_positive',
    ),
    '#states' => array(
      'required' => array(
        ':input[name="archidora_max_age"]' => array(
          'value' => -1,
        ),
      ),
      'visible' => array(
        ':input[name="archidora_max_age"]' => array(
          'value' => -1,
        ),
      ),
    ),
  );

  return system_settings_form($form);
}

/**
 * Element validate callback; require a value, dependant on another field.
 */
function archidora_custom_max_age_provided($element, &$form_state) {
  if ($form_state['values']['archidora_max_age'] == -1 && $element['#value'] == '') {
    form_error($element, t('%name must be provided.', array(
      '%name' => $element['#title'],
    )));
  }
}
