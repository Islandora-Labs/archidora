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

  $form['archidora_archivematica_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Archivematica URL'),
    '#default_value' => variable_get('archidora_archivematica_url', 'http://localhost'),
  );
  $form['archidora_user'] = array(
    '#type' => 'textfield',
    '#title' => t('Archivematica User'),
    '#description' => t('The username to use to connect to Archivematica.'),
    '#default_value' => variable_get('archidora_user', 'archidora'),
  );
  $form['archidora_pass'] = array(
    '#type' => 'password',
    '#title' => t('Archivematica Password'),
    '#description' => t('The password for the user indicated above.'),
    '#default_value' => variable_get('archidora_pass', '');
  );

  return system_settings_form($form);
}
