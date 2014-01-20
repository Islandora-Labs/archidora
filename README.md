SUMMARY
-------

Archivematica Integration Module

REQUIREMENTS
------------

Depends on [Islandora](http://github.com/Islandora/islandora).

INSTALLATION
------------

Before installing Islandora the XACML policies located in the policies folder
should be copied into the Fedora global XACML policies folder. This will allow
"authenticated users" in Drupal to access Fedora API-M functions.

You will also have to remove some default policies if you want full functionality as well.

Remove deny-purge-datastream-if-active-or-inactive.xml to allow for purging of datastream versions.

CONFIGURATION
-------------

The base URL, user and user's API key will have to be entered into this modules
admin screen, at `admin/islandora/archidora`.

