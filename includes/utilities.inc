<?php

/**
 * @file
 * Utility functions.
 */

/**
 * Get the parent for the given object, for which to create an AIP.
 *
 * @param AbstractObject $object
 *   The object for which to grab the parent... Used when looking up to which
 *   AIP this object should be added.
 *
 * @return string|FALSE
 *   The first eligible parent's PID, or FALSE if we couldn't find any.
 */
function archidora_get_parent(AbstractObject $object) {
  $map = function ($relationship) {
    return $relationship['object']['value'];
  };

  $rels = array(
    FEDORA_RELS_EXT_URI => array(
      'isMemberOfCollection',
      'isMemberOf',
    ),
  );
  $memberships = array();
  foreach ($rels as $uri => $preds) {
    foreach ($preds as $pred) {
      $parent_pids = array_map($map, $object->relationships->get($uri, $pred));
      $memberships = array_merge($memberships, $parent_pids);
    }
  }

  return reset($memberships);
}

/**
 * Get a list of the content models which should not add to AIPs.
 *
 * @return array
 *   An array of string representing content models, without the "info:fedora/"
 *   bit.
 */
function archidora_skip_content_models() {
  return array(
    'islandora:collectionCModel',
    'islandora:newspaperCModel',
    'islandora:newspaperIssueCModel',
    'islandora:bookCModel',
    'islandora:compoundCModel',
  );
}

/**
 * Checks if a PID should be enqueued for archivematica.
 * @todo implement using hybrid approach as query on isMemberOf but ITQL walk on
 * isMemberOfCollection.
 */
function archidora_should_auto_enqueue($pid) {
  return !FALSE;
}

/**
 * Sets wether an object's children shouldn't be auto sent to archivematica.
 *
 * @param AbstractObject $object
 *   The object to set the RELS on.
 * @param bool $disable
 *   FALSE if the object's children should be automatically uploaded.
 *   TRUE if the object's children should not be automatically uploaded.
 */
function archidora_set_auto_enqueue_disabled(AbstractObject $object, $disable) {
  $relations = $object->relationships;
  $relations->autoCommit = FALSE;

  $relations->remove(
    ARCHIDORA_RDF_URI,
    ARCHIDORA_DO_NOT_AUTO_UPLOAD_CHILDREN
  );
  $relations->add(
    ARCHIDORA_RDF_URI,
    ARCHIDORA_DO_NOT_AUTO_UPLOAD_CHILDREN,
    $disable ? 'TRUE' : 'FALSE',
    RELS_TYPE_PLAIN_LITERAL
  );

  $relations->commitRelationships();
}

/**
 * Wether an object's children shouldn't be auto sent to archivematica.
 */
function archidora_auto_enqueue_disabled(AbstractObject $object) {
  $retrieved_rel = $object->relationships->get(
    ARCHIDORA_RDF_URI,
    ARCHIDORA_DO_NOT_AUTO_UPLOAD_CHILDREN
  );
  if ($retrieved_rel) {
    $reset_rel = reset($retrieved_rel);
    return ($reset_rel['object']['value'] == 'TRUE');
  }
  return FALSE;
}
