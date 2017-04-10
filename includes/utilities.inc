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
 */
function archidora_should_auto_enqueue($pid) {

//    SELECT ?o FROM <info:edu.si.fedora#ri>
//WHERE {
//        ?parent <http://islandora.ca/archivematica#doNotAutoUploadChildren> ?o .
//  optional {<info:fedora/islandora:33460> <info:fedora/fedora-system:def/relations-external#isMemberOfCollection> ?parent}
//  optional {<info:fedora/islandora:33460> <info:fedora/fedora-system:def/relations-external#isMemberOf> ?parent}
//  optional {<info:fedora/islandora:33460> <info:fedora/fedora-system:def/relations-external#isPartOf> ?parent}
//}

  $ri = islandora_get_tuque_connection()->repository->ri;
  $predicates = array(
    'isMemberOf',
    'isMemberOfCollection',
  );
  if (module_exists('islandora_compound_object')) {
    $predicates[] = variable_get('islandora_compound_object_relationship', 'isConstituentOf');
  }

  $sparql_enabled = variable_get('archidora_use_sparql');

  $sparql_query = 'SELECT ?disabled FROM  <info:edu.si.fedora#ri> WHERE {'.
      '?parent <'  . ARCHIDORA_RDF_URI . ARCHIDORA_DO_NOT_AUTO_UPLOAD_CHILDREN . '> ?disabled .';
      foreach ($predicates as $predicate){
          $sparql_query .= ' optional {<info:fedora/' . trim($pid) . '> <fedora-rels-ext:' . $predicate . '> ?parent}';
      }
      $sparql_query .= '}';

 drupal_set_message("Is sparql enabled? " . $sparql_enabled, 'status');

  $query = 'select $parent
    subquery (
      select $disabled from <#ri> where (
        $parent <' . ARCHIDORA_RDF_URI . ARCHIDORA_DO_NOT_AUTO_UPLOAD_CHILDREN . '> $disabled
      )
    )
    from <#ri> where (';
  $walks = array();
  $leafs = array($pid);
  $last_leafs = array();
  while (array_diff($leafs, $last_leafs)) {
    $last_leafs = $leafs;
    foreach ($predicates as $predicate) {
      foreach ($leafs as $current_pid) {
        $walks[] = '
          walk
            (
              <info:fedora/' . $current_pid . '> <fedora-rels-ext:' . $predicate . '> $parent
                and $child <fedora-rels-ext:' . $predicate . '> $parent
            )
        ';
      }
    }
    $current_query = $query . implode(' or ', $walks);
    $current_query .= ')';
    $results = $ri->itqlQuery($current_query);
    $leafs = array();
    foreach ($results as $result) {
      if ($result['disabled']['value'] === 'TRUE') {
        return FALSE;
      }
      $leafs[] = $result['parent']['value'];
    }
    $leafs = array_unique($leafs);
  }
  return TRUE;
}

/**
 * Sets whether an object's children shouldn't be auto sent to archivematica.
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
 * Whether an object's children shouldn't be auto sent to archivematica.
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

/**
 * Purge a datastream if it exists.
 *
 * @param string $pid
 *   The PID of the object to purge the datastream from.
 * @param string $dsid
 *   The DSID of the datastream to purge.
 */
function archidora_purge_datastream($pid, $dsid) {
  $object = islandora_object_load($pid);
  if ($object && isset($object[$dsid])) {
    $object->purgeDatastream($dsid);
  }
}

/**
 * Objects that have been flagged for deletion in the aggregate.
 *
 * @param string $pid
 *   The PID of the aggregate object.
 * @param int $limit
 *   The limit for the query.
 * @param int $offset
 *   The offset for the query.
 *
 * @return array
 *   As returned by Tuque's sparqlQuery function with results of pid and label.
 */
function archidora_flagged_objs($pid, $limit, $offset) {
  $objects_query = archidora_flagged_objs_query($pid);
  $objects_query .= "LIMIT $limit OFFSET $offset";
  $tuque = islandora_get_tuque_connection();
  return $tuque->repository->ri->sparqlQuery($objects_query);
}

/**
 * Count of objects that have been flagged for deletion in the aggregate.
 *
 * @param string $pid
 *   The PID of the aggregate object.
 *
 * @return int
 *   Count of flagged objects.
 */
function archidora_total_flagged_objs($pid) {
  $objects_query = archidora_flagged_objs_query($pid);
  $tuque = islandora_get_tuque_connection();
  return $tuque->repository->ri->countQuery($objects_query, 'sparql');
}

/**
 * A SPARQL query string for getting flagged objects based on an a parent PID.
 */
function archidora_flagged_objs_query($pid) {
  return <<<EOQ
SELECT ?object ?label FROM <#ri> WHERE {
  {
  ?object <fedora-rels-ext:isMemberOf> <info:fedora/$pid>
  }
  UNION
  {
  ?object <fedora-rels-ext:isMemberOfCollection> <info:fedora/$pid>
  }
  ?object <fedora-view:disseminates> [<http://islandora.ca/archivematica#isReadyForDeletion> 'TRUE'] ;
          <fedora-model:label> ?label ;
          <fedora-view:disseminates> ?ds .
  ?ds <fedora-view:disseminationType> <info:fedora/*/OBJ>
}
EOQ;
}
