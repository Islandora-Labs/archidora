<?php

class ArchidoraArchivematicaQueue implements DrupalReliableQueueInterface {
  const LOCK_NAME = 'archidora_queue_claim';

  protected function __construct() {
    // No-op.
  }

  protected $instance = NULL;
  public static function getInstance() {
    if (static::$instance === NULL) {
      static::$instance = new static();
    }

    return static::$instance;
  }

  public function createQueue() {
    // No-op.
  }

  public function deleteQueue() {
    // Delete everything in the queue...  KABOOM!
    db_delete('archidora_aip_queue')
      ->execute();
  }

  /**
   * Add new item to queue...
   *
   * @param object $data
   *   An object with the properties:
   *   - object: An AbstractObject for which to create an entry.
   *   - aip: An optional key, indicating which AIP the object should be made
   *     a part of. When not provided, we should look it up.
   */
  public function createItem($data) {
    // Add the object to the queue...
    $to_insert = array(
      'object' => $data->object->id,
      'last_modified_date' => $data->object->lastModifiedDate->getTimestamp(),
    );
    if (isset($data->aip) && !empty($data->aip)) {
      $to_insert['aip'] = $data->aip;
    }

    db_insert('archidora_aip_queue')
      ->fields($to_insert)
      ->execute();
  }

  /**
   * Helper function to get a lock.
   */
  protected function getClaimLock($timeout = 30.0) {
    return lock_acquire(self::LOCK_NAME) || (lock_wait(self::LOCK_NAME) && lock_acquire(self::LOCK_NAME));
  }

  /**
   * Helper function to release the lock.
   */
  protected function releaseClaimLock() {
    return lock_release(self::LOCK_NAME);
  }

  /**
   * Helper function to get a basic select query.
   */
  protected function getUnclaimedSelectQuery() {
    return db_select('archidora_aip_queue', 'q')
      ->fields('q', array())
      ->condition('claimed', 0);
  }

  /**
   * @return array
   *   An object reflecting the columns of the archidora_aip_queue table.
   */
  public function claimItem() {
    // Grab "claim" lock, grab unprocessed and unclaimed item from queue,
    // mark as claimed in separate table.
    if ($this->getClaimLock()) {
      // Grab unprocessed, unclaimed item.
      $result = $this->getUnclaimedSelectQuery()
        ->range(0, 1)
        ->execute()
        ->fetchObject();
      $result->claimed = 1;
      if ($result) {
        // Mark item as claimed.
        db_update('archidora_aip_queue')
          ->fields('claimed' => $result->claimed)
          ->condition('object' => $result->object)
          ->condition('last_modified_date' => $result->last_modified_date)
          ->execute();
        $this->releaseClaimLock();

        //  Return the item.
        return $result;
      }
      $this->releaseClaimLock();
    }

    return FALSE;
  }

  public function deleteItem($item) {
    // Delete from queue.
    db_delete('archidora_aip_queue')
      ->condition('object' => $item->object)
      ->condition('last_modified_date' => $item->last_modified_date)
      ->execute();
  }

  public function releaseItem($item) {
    if ($this->getClaimLock()) {
      // Grab "claim" lock, remove claim.
      $item->claimed = 0;
      db_update('archidora_aip_queue')
        ->fields('claimed' => $item->claimed)
        ->condition('object' => $item->object)
        ->condition('last_modified_date' => $item->last_modified_date)
        ->execute();
      $this->releaseClaimLock();
    }

    return FALSE;
  }

  public function numberOfItems() {
    // Get the number of unclaimed items.
    return $this->getUnclaimedSelectQuery()
      ->countQuery()
      ->execute()
      ->fetchField();
  }
}
