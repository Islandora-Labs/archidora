<?php

/**
 * @file
 * Functions associated with derivative creation.
 */

/**
 * Derivative callback; send the object to Archivematica.
 */
function archidora_derivative_callback($object, $force = FALSE) {
  module_load_include('inc', 'archidora', 'includes/archivematica');
  if ($force || !archidora_get_em_iri_from_object($object)) {
    archidora_send_to_archivematica($object);
  }
}
