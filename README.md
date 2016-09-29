SUMMARY
-------

Archivematica Integration Module

REQUIREMENTS
------------

Depends on [Islandora](http://github.com/Islandora/islandora).

INSTALLATION
------------

Install as any other Drupal module.

CONFIGURATION
-------------

The Archivematica's Storage Service URL, user and user's API key will have to
be entered into this modules admin screen, at `admin/islandora/archidora`, and
a Deposit Location selected. An API token should also be generated to allow
Archivematica to call back to Islandora.

In Fedora, the domain name of the server will have to be configured properly
as well as a user created for Archivematica. The user created in Fedora will
have to be entered in Archivematica's configuration to be able to authenticate
with Fedora, to be able to pull datastreams.

Archivematica may also be configured to call back to Islandora to delete the
high-res "OBJ" datastreams.

Note: As a side-effect of using
[Cron Queues](https://api.drupal.org/api/drupal/modules!system!system.api.php/function/hook_cron_queue_info/7),
the submission of objects to Archivematica may not complete during any one
invocation of Cron.

Drush

drush asca --target=john:test --children=no