Introduction
------------

Drush tool to dump/update CTools (and some other) exportables.

To a site developer, CTools exportables (which includes Panels, Page
Manager pages and Strongarm variables) and default Views, is a godsend
for creating sites and then storing the setup in code. However,
exporting the objects and saving them in the right files is a bit of a
chore, and it is here Drush EM helps out by providing some simple
commands.

In a way it's an alternative to using the Features module, but which
leaves package and release management up to the developer.

Due to the current status and nature of CTools, Views and other
modules Drush EM supports, it currently contains quite a bit of module
specific code. Hopefully things will stabilise, so new modules should
be supported out of the box.

Usage
-----

Most commands needs to be run from the directory of the module being
worked on.

Drush EM doesn't add the hook_ctools_plugin_api or other hooks that
needs to be implemented for the relevant module to discover the
exportables. These needs to be added manually, but it's only done once
per module/exportable type. In the future Drush EM might print a
suggested implementation of the hook.

Exportables are identified by their type and name, for example:

views:frontpage       The view named 'frontpage'.
variables:site_name   The variable 'site_name'.

Either part can be shortened to any unique sub-string, for instance:

vi:front        Same as views:frontpage, if there's no other type that
                contains 'vi' and no other view containing 'front'.
var:te_name     Usually equivalent to variables:site_name.

Drush EM will complain and abort if more than one type or exportable
match.

Simple pattern matching can also be used to specify exportables, and
these *are* allowed to match multiple exportables. Example:

var:site_*  Dump all variables that start with 'site_'.
*:*node     Dump all exportables whose name ends with 'node'.

The easiest way to find out what exportable types there is, is using
the em-list command. Note, however, that it doesn't list variables,
even if the Strongarm module is enabled, as it would quickly become a
very long list.

Command overview:

em-list:
   List site exportables that's currently not dumped to code.

em-status:
   Show status (overridden or not) of exportables in the current
   module, or all modules.

em-dump:
   Dump or update exportables into the current module, and remove the
   changes from the site database.

em-update:
   Update the modules exports with what's currently in the site
   database. This is a quick way to update the module with changes to
   the site.

em-remove:
   Remove an exportable from the current module.

em-revert:
   Revert an exportable. Removes any changes to the exportable from
   the database.

em-diff:
   Show differences between code and database.
