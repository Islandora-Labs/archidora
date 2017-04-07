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
  $form['archivematica']['archidora_archivematica_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Archivematica Storage Service Base URL'),
    '#description' => t('The base URL to use to contact the Archivematica Storage Service. Should include the scheme and the port.'),
    '#default_value' => variable_get('archidora_archivematica_url', 'http://localhost:8000'),
    '#ajax' => array(
      'wrapper' => 'deposit-location-wrapper',
      'callback' => 'archidora_ajax_deposit_location',
    ),
  );
  $options = archidora_admin_form_get_location_options(isset($form_state['values']['archidora_archivematica_url']) ?
    $form_state['values']['archidora_archivematica_url'] :
    variable_get('archidora_archivematica_url', '')
  );

  $form['archivematica']['wrapper'] = array(
    '#prefix' => '<div id="deposit-location-wrapper">',
    '#suffix' => '</div>',
    'archidora_deposit_location' => array(
      '#type' => 'select',
      '#access' => !empty($options),
      '#title' => t('Deposit Location'),
      '#description' => t('The location in which AIPs will be created.'),
      '#default_value' => variable_get('archidora_deposit_location', ''),
      '#options' => $options,
    ),
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
    '#description' => t('The API key for the user indicated above.'),
    '#default_value' => variable_get('archidora_api_key', ''),
  );
  $form['archivematica']['archidora_islandora_api_key'] = array(
    '#type' => 'item',
    '#title' => t('Islandora Archivematica integration API Key'),
    '#description' => t('The API key generated via Islandora.'),
    '#markup' => variable_get('archidora_islandora_api_key', ''),
  );
  $form['archivematica']['archidora_islandora_api_key_submit'] = array(
    '#type' => 'submit',
    '#value' => t('Generate'),
    '#submit' => array('archidora_generate_islandora_api_key'),
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
    '#default_value' => variable_get('archidora_max_age_custom', ''),
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
  $size_options = array(
    1048576,
    4 * 1048576,
    16 * 1048576,
    64 * 1048576,
    256 * 1048576,
    512 * 1048576,
    1073741824,
    1073741824 * 4,
    1073741824 * 16,
    1073741824 * 64,
  );
  $size_options = array(
    -1 => t('Custom'),
  ) + array_map('format_size', drupal_map_assoc($size_options));
  $form['archidora_max_size'] = array(
    '#type' => 'select',
    '#title' => t('AIP Max Size'),
    '#description' => t('New objects will not be added to an AIP after it reaches this size, and it will be flagged for finalization at the next run.'),
    '#options' => $size_options,
    '#default_value' => variable_get('archidora_max_size', 1073741824),
  );
  $form['archidora_max_size_custom'] = array(
    '#type' => 'textfield',
    '#title' => t('Custom max size'),
    '#description' => t('An integer expression in bytes.'),
    '#default_value' => variable_get('archidora_max_size_custom', ''),
    '#element_validate' => array(
      'archidora_custom_max_size_provided',
      'element_validate_integer_positive',
    ),
    '#states' => array(
      'required' => array(
        ':input[name="archidora_max_size"]' => array(
          'value' => -1,
        ),
      ),
      'visible' => array(
        ':input[name="archidora_max_size"]' => array(
          'value' => -1,
        ),
      ),
    ),
  );

  $cron_options = array(
    15,
    30,
    60,
    300,
    600,
    1800,
    3600,
  );
  $cron_options = array(
    -1 => t('Custom'),
  ) + array_map('format_interval', drupal_map_assoc($cron_options));
  $form['archidora_cron_time'] = array(
    '#type' => 'select',
    '#title' => t('Cron Time'),
    '#description' => t('The amount of time for which the queue of items will be allowed to process, at each cron invocation.'),
    '#options' => $cron_options,
    '#default_value' => variable_get('archidora_cron_time', 600),
  );
  $form['archidora_cron_time_custom'] = array(
    '#type' => 'textfield',
    '#title' => t('Custom cron time'),
    '#description' => t('An interval, expressed as seconds.'),
    '#default_value' => variable_get('archidora_cron_time_custom', ''),
    '#element_validate' => array(
      'archidora_custom_cron_time_provided',
      'element_validate_integer_positive',
    ),
    '#states' => array(
      'required' => array(
        ':input[name="archidora_cron_time"]' => array(
          'value' => -1,
        ),
      ),
      'visible' => array(
        ':input[name="archidora_cron_time"]' => array(
          'value' => -1,
        ),
      ),
    ),
  );

   $form['archidora_use_sparql'] = array(
       '#type' => 'checkbox',
       '#title' => t('Check to enable SPARQL'),
       '#description' => t('Leave unchecked for Mulgara triplestore'),
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

/**
 * Element validate callback; require a value, dependant on another field.
 */
function archidora_custom_max_size_provided($element, &$form_state) {
  if ($form_state['values']['archidora_max_size'] == -1 && $element['#value'] == '') {
    form_error($element, t('%name must be provided.', array(
      '%name' => $element['#title'],
    )));
  }
}

/**
 * Element validate callback; require a value, dependant on another field.
 */
function archidora_custom_cron_time_provided($element, &$form_state) {
  if ($form_state['values']['archidora_cron_time'] == -1 && $element['#value'] == '') {
    form_error($element, t('%name must be provided.', array(
      '%name' => $element['#title'],
    )));
  }
}

/**
 * Submit handler to generate an API key for use in Archivematica.
 */
function archidora_generate_islandora_api_key() {
  variable_set('archidora_islandora_api_key', bin2hex(drupal_random_bytes(32)));
}

/**
 * AJAX form callback.
 */
function archidora_ajax_deposit_location($form, $form_state) {
  return $form['archivematica']['wrapper'];
}

/**
 * Get the available SWORD endpoints from the given storage service.
 *
 * @param string $base_ss_url
 *   Base URL to the storage service.
 *
 * @return array
 *   An associative array mapping deposit location URIs to translated
 *   human-readable strings.
 */
function archidora_admin_form_get_location_options($base_ss_url) {
  $to_return = array();
  $response = drupal_http_request("$base_ss_url/api/v1/sword");
  if ($response->code == 200) {
    $doc = new DOMDocument();
    $doc->loadXML($response->data);
    $xpath = new DOMXPath($doc);
    $xpath->registerNamespace('s', 'http://www.w3.org/2007/app');
    $xpath->registerNamespace('atom', 'http://www.w3.org/2005/Atom');

    foreach ($xpath->query('/s:service/s:workspace/s:collection[@href]') as $location) {
      $uri = $location->getAttribute('href');
      $titles = $xpath->query('atom:title', $location);
      $title = $titles->length > 0 ?
        $titles->item(0)->nodeValue :
        'N/A';

      $to_return[$uri] = t('!title (!uri)', array(
        '!title' => $title,
        '!uri' => $uri,
      ));
    }
  }
  else {
    drupal_set_message(t('Error contacting @url, HTTP code @code.', array(
      '@url' => $base_ss_url,
      '@code' => $response->code,
    )), 'warning', FALSE);
  }

  return $to_return;
}
