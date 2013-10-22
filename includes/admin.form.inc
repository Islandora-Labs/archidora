<?php

function archidora_admin_form($form, &$form_state) {
  form_load_include($form_state, 'inc', 'archidora', 'includes/admin.form');

  $form['archidora_archivematica_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Archivematica URL'),
    '#default_value' => variable_get('archidora_archivematica_url', 'http://localhost'),
  );

  return system_settings_form($form);
}
