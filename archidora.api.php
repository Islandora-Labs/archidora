<?php

/**
 * @file
 * Descriptions of hooks exposed by this modules.
 */

/**
 * Allow modules to react to objects being sent to archivematica.
 *
 * @param AbstractObject $object
 *   The object which was sent.
 * @param object $aip
 *   A stdClass object to which the object was sent.
 */
function hook_archidora_object_sent(AbstractObject $object, $aip) {
}

/**
 * hook_archidora_object_sent(), except specific to content models.
 */
function hook_CMODEL_PID_archidora_object_sent(AbstractObject $object, $aip) {
}
