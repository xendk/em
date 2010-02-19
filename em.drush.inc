<?php

/*
em status: * bedre listing?
list hvilke der er overridden

em revert: * funker. Kunne have bedre argumenter (patterns?)
* understøttelse af flere typer

em dump: funker, kunne have bedre argumenter
exporter navngivne til fil, bevar eksisterende (for så vidt muligt)

em del: virker
slet given export

em update: virker.
reexport alle overridden

em list: virker. bedre listing?
vis alle exportables vi kan klare

em diff:
vis forskelle (hvis det er muligt)

 */

define('EM_STATUS_IN_CODE', 0x01);
define('EM_STATUS_IN_DATABASE', 0x02);

function em_drush_command() {
  $items['em status'] = array(
    // We're skipping the customary drush_ prefix, as it increases the chance
    // of colliding with a hook.
    'callback' => 'em_status',
    'description' => 'Show exportable status of current module.',
    'arguments' => array(
      'name' => 'The name of the field whose instances you wish to compare. Mandatory.',
    ),
    'options' => array(
      '-o' => 'Only show overridden.',
    ),
    'examples' => array(
      'em status field_teaser' => 'Compare all instances of field_teaser.',
    ),
    // 'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_SITE,
  );

  $items['em revert'] = array(
    'callback' => 'em_revert',
    'description' => 'Revert an exportable',
    'arguments' => array(
      'exportables' => 'Space delimited list of exportables to revert, or \'all\' to revert all module defined exportables.'
    ),
    'examples' => array(
      'em revert views:frontpage' => 'Revert the view "frontpage".',
    ),
  );

  $items['em dump'] = array(
    'callback' => 'em_dump',
    'description' => 'Dump an exportable to the current module',
    'arguments' => array(
      'exportables' => 'Space delimited list of exportables to add.'
    ),
    'examples' => array(
      'em dump views:frontpage' => 'Dump the view "frontpage" to the relevant file.',
    ),
  );

  $items['em remove'] = array(
    'callback' => 'em_remove',
    'description' => 'Remove an exportable from the current module',
    'arguments' => array(
      'exportables' => 'Space delimited list of exportables to remove.'
    ),
    'examples' => array(
      'em remove views:frontpage' => 'Remove the "frontpage" view from the relevant file.',
    ),
  );

  $items['em update'] = array(
    'callback' => 'em_update',
    'description' => 'Update module exportables from site',
  );

  $items['em list'] = array(
    'callback' => 'em_list',
    'description' => 'List site exportables',
  );
  return $items;
}

/**
 * Implementation of hook_drush_help().
 */
function em_drush_help($section) {
  switch ($section) {
    case 'drush:em status':
      return dt('Lists all exportables defined by the current module, and whether they are overridden or not.');
    case 'drush:em revert':
      return dt('Reverts the given exportables to their code defined state.');
    case 'drush:em dump':
      return dt('Dump an exportable to the current module. You might need to add a function like hook_ctools_api to your module file.');
    case 'drush:em remove':
      return dt('Remove an exportable from the current module.');
    case 'drush:em update':
      return dt('Updates all overridden exportables defined by the current module with the settings from the site, and reverts them afterwards.');
    case 'drush:em list':
      return dt('Lists all site exportables that is not defined in code (overridden or not).');
  }
}

/**
 * Defines aliases for exportable types.
 * TODO: Actually use this.
 */
function em_tables() {
  static $tables;
  if (!$tables) {
    $tables = array(
      'pages' => 'page_manager_pages',
    );
  }
  return $tables;
}

/**
 * Defines out exportable types.
 */
function em_export($table = NULL, $key = NULL) {
  static $export_types = NULL;
  static $overrides = array(
    'page_manager_pages' => array(
      'pre export' => 'page_manager_get_tasks',
      'delete callback' => 'em_delete_page_manager_pages',
    ),
    'page_manager_handlers' => array(
      'delete callback' => 'page_manager_delete_task_handler',
    ),
    'panels_mini' => array(
      'delete callback' => 'panels_mini_delete',
    ),
    'variable' => array(
      // Stongarm 2.0 RC1 defines a non-existent export callback.
      'export callback' => 'em_export_strongarm',
      'status callback' => 'em_strongarm_status',
      'delete callback' => 'em_strongarm_delete',
    ),
  );

  if (!$export_types) {
    // TODO: predefined non-ctools types.
    $export_types = array();
    if (module_exists('views')) {
      views_include('view');
      $export_types['views'] = array(
        'api' => 'views_default',
        'identifier' => 'view',
        'key' => 'name',
        'list callback' => 'views_get_all_views',
        'default hook' => 'views_default_views',
        'load callback' => 'em_get_views',
        // TODO: views_view_exportables might be more right.
        'export callback' => 'em_export_view',
        'status callback' => 'em_view_status',
        'delete callback' => 'em_view_delete',
      );
    }

    if (module_exists('field_gatekeeper')) {
      $export_types['field_gatekeeper'] = array(
        'api' => 'field_gatekeeper',
        'identifier' => 'field',
        'key' => 'field_name',
        'list callback' => 'em_fg_get_all_gatekeeper',
        'default hook' => 'field_gatekeeper',
        'load callback' => 'em_fg_load_gatekeeper',
        //'export callback' => '',
        'to hook code callback' => 'em_fg_gatekeeper_to_hook_code',
        'status callback' => 'em_fg_gatekeeper_status',
        'delete callback' => 'em_fg_gatekeeper_delete',
        'is array' => TRUE,
      );

      $export_types['field_instances'] = array(
        'api' => 'field_instances',
        'identifier' => 'field',
        'key' => 'name',
        'list callback' => 'em_fg_get_all_instances',
        'default hook' => 'field_instances',
        'load callback' => 'em_fg_load_instances',
        //'export callback' => '',
        'to hook code callback' => 'em_fg_instances_to_hook_code',
        'status callback' => 'em_fg_instance_status',
        'delete callback' => 'em_fg_instance_delete',
        'is array' => TRUE,
      );
    }

    if (module_exists('ctools')) {
      // Find out what ctools exportables that's available.
      ctools_include('export');
      $schemas = ctools_export_get_schemas(TRUE);

      foreach ($schemas as $etable => $schema) {
        if ($schema['export']['api']) {
          $export_types[$etable] = array(
            'api' => $schema['export']['api']['api'],
            'identifier' => $schema['export']['identifier'],
            'key' => $schema['export']['key'],
            'export callback' => $schema['export']['export callback'],
            'default hook' => $schema['export']['default hook'],
            'list callback' => $schema['export']['list callback'],
            'to hook code callback' => $schema['export']['to hook code callback'],
            'load callback' => 'ctools_export_load_object',
            'load arguments' => array($etable, 'names'),
            'status callback' => 'em_ctools_status',
          );
          if ($overrides[$etable]) {
             $export_types[$etable]= $overrides[$etable] + $export_types[$etable];
          }
        }
        else {
          drush_print_r(array($etable, $schema['export']));
        }
      }
    }
  }

  if ($key) {
    return $export_types[$table][$key];
  } elseif ($table) {
    return $export_types[$table];
  }
  return $export_types;
}

function em_delete_page_manager_pages($object) {
  static $loaded;
  // For some reason, the right task include might not be loaded, so we tickle it here.
  if (!$loaded) {
    page_manager_get_tasks();
    $loaded = TRUE;
  }
  page_manager_page_delete($object);
  return;
}

function em_fg_get_all_gatekeeper() {
  module_load_include('inc', 'content', '/includes/content.crud');
  $fields = array();
  // Load fields in DB from CCK.
  $all_fields = content_field_instance_read(NULL, TRUE);
  foreach ($all_fields as $field) {
    $fields[$field['field_name']] = field_gatekeeper_base_clean($field);
  }
  // Overlay module defined.
  $gw_fields = field_gatekeeper_get_field_gatekeeper();
  foreach ($gw_fields as $field) {
    $fields[$field['field_name']] = field_gatekeeper_base_clean($field);
  }
  return $fields;
}

function em_fg_get_all_instances() {
  module_load_include('inc', 'content', '/includes/content.crud');
  $fields = array();
  // Load fields in DB from CCK.
  $all_fields = content_field_instance_read(NULL, TRUE);
  foreach ($all_fields as $field) {
    $field = field_gatekeeper_instance_clean($field);
    $field['name'] = $field['type_name'] . '-' . $field['field_name'];
    $fields[$field['name']] = $field;
  }
  // Overlay module defined.
  $gw_fields = field_gatekeeper_get_field_instances();
  foreach ($gw_fields as $field) {
    $fields[$field['name']] = field_gatekeeper_instance_clean($field);
  }
  return $fields;
}

function em_fg_load_gatekeeper($names) {
  $all_fields = em_fg_get_all_gatekeeper();
  $fields = array();
  foreach ($names as $name) {
    if ($all_fields[$name]) {
      $fields[$name] = $all_fields[$name];
    }
  }
  return $fields;
}

function em_fg_load_instances($names) {
  $all_fields = em_fg_get_all_instances();
  $fields = array();
  foreach ($names as $name) {
    if ($all_fields[$name]) {
      $fields[$name] = $all_fields[$name];
    }
  }
  return $fields;
}

function em_fg_gatekeeper_status($field) {
  module_load_include('inc', 'content', '/includes/content.crud');
  // load all instances of the field
  $code_defined = field_gatekeeper_get_field_gatekeeper();
  if (!$code_defined[$field['field_name']]) {
    return EM_STATUS_IN_DATABASE;
  }

  $options = array('field_name' => $field['field_name']);
  $instances = content_field_instance_read($options);

  foreach ($instances as $instance) {
    $instance['locked'] = '1';
    $diffs = _field_gatekeeper_array_rdiff($field, field_gatekeeper_base_clean($instance));
    if ($diffs) {
      drush_print_r(array($field['allowed_values'], $instance['allowed_values']));
      return EM_STATUS_IN_DATABASE | EM_STATUS_IN_CODE;
    }
  }
  return EM_STATUS_IN_CODE;
}

function em_fg_instance_status($field) {
  module_load_include('inc', 'content', '/includes/content.crud');
  // load instance of the field
  $options = array('field_name' => $field['field_name'], 'type_name' => $field['type_name']);

  $code_defined = field_gatekeeper_get_field_instances();
  if (!$code_defined[$field['name']]) {
    return EM_STATUS_IN_DATABASE;
  }

  $instances = content_field_instance_read($options);
  if (!$instances) {
    return EM_STATUS_IN_CODE;
  }

  foreach ($instances as $instance) {
    $instance['locked'] = '1';
    $diffs = _field_gatekeeper_array_rdiff($field, field_gatekeeper_instance_clean($instance));
    if ($diffs) {
       return EM_STATUS_IN_DATABASE | EM_STATUS_IN_CODE;
    }
  }
  return EM_STATUS_IN_CODE;
}

function em_fg_gatekeeper_delete($field) {
  drush_print(dt('Reverting !field_name will also revert all instances of the field.', array('!field_name' => $field['field_name'])));
  if(!drush_confirm(dt('Do you want to do this?'))) {
    return;
  }
  field_gatekeeper($field);
}


function em_fg_instance_delete($field) {
  field_gatekeeper($field);
}

function em_fg_gatekeeper_to_hook_code($objects, $module) {
  $export = em_export('field_gatekeeper');
  $output = "/**\n";
  $output .= " * Implementation of hook_{$export['default hook']}().\n";
  $output .= " */\n";
  $output .= "function " . $module . "_{$export['default hook']}() {\n";
  foreach ($objects as $name) {
    module_load_include('inc', 'content', '/includes/content.crud');
    $instances = content_field_instance_read(array('field_name' => $name));
    $instance = $instances[0];
    $instance['locked'] = 1;

    $instance = field_gatekeeper_base_clean($instance);
    $dump = "  \${$export['identifier']} = ";
    $dump .= em_var_export($instance, '  ');
    $dump .= ";\n";
    $output .= $dump;

    $output .= "  \${$export['identifier']}s['" . check_plain($instance['field_name']) . "'] = \${$export['identifier']};\n\n";
  }
  $output .= "  return \${$export['identifier']}s;\n";
  $output .= "}\n";
  return $output;
};

function em_fg_instances_to_hook_code($objects, $module) {
  $export = em_export('field_instances');
  $output = "/**\n";
  $output .= " * Implementation of hook_{$export['default hook']}().\n";
  $output .= " */\n";
  $output .= "function " . $module . "_{$export['default hook']}() {\n";

  foreach ($objects as $name) {
    list($type, $field) = explode('-', $name);
    if (!$field) {
      continue;
    }
    module_load_include('inc', 'content', '/includes/content.crud');
    $instances = content_field_instance_read(array('field_name' => $field,
                                                   'type_name' => $type));
    $instance = $instances[0];
    $instance['locked'] = 1;

    $instance = field_gatekeeper_instance_clean($instance);
    $dump = "  \${$export['identifier']} = ";
    $dump .= em_var_export($instance, '  ');
    $dump .= ";\n";
    $output .= $dump;

    $output .= "  \${$export['identifier']}s['" . check_plain($name) . "'] = \${$export['identifier']};\n\n";
  }
  $output .= "  return \${$export['identifier']}s;\n";
  $output .= "}\n";
  return $output;
}


/**
 * Helper function to load multiple views.
 */
function em_get_views($names) {
  $res = array();
  foreach ($names as $name) {
    $view = views_get_view($name);
    $res[$view->name] = $view;
  }
  return $res;
}

/**
 * Helper function to export a view.
 */
function em_export_view($view, $indent) {
  return $view->export($indent);
}

function em_export_strongarm($variable, $indent = '') {
  return ctools_export_object('variable', $variable, $indent);
}

function em_view_status($export) {
  if ($export->type == t('Overridden')) {
    return EM_STATUS_IN_DATABASE | EM_STATUS_IN_CODE;
  } elseif ($export->type == t('Standard')) {
    return EM_STATUS_IN_CODE;
  }
  // t('Normal')
  return EM_STATUS_IN_DATABASE;
}

function em_ctools_status($export) {
  $status = 0;
  if ($export->export_type & EXPORT_IN_CODE) {
    $status = EM_STATUS_IN_CODE;
  }
  if ($export->export_type & EXPORT_IN_DATABASE) {
    $status = $status | EM_STATUS_IN_DATABASE;
  }
  return $status;
}

function em_strongarm_status($export) {
  // Due to the nature of strongarm, exports are never overridden.
  return EM_STATUS_IN_CODE;
}

/**
 * Helper function to delete view.
 */
function em_view_delete($view) {
  $view->delete();
}

function em_strongarm_delete($export) {
  // As strongarm can't be overridden, reverting is a noop.
  return;
}

function em_status() {
  if (!$module = em_get_module()) {
    return FALSE;
  }

  // Now, find out what exportables that's available.
  $export_schemas = em_export();
  $quick = drush_get_option('o', FALSE);
  foreach ($export_schemas as $table => $export) {
    $site_exports = array();
    $module_exports = em_get_module_exports($module, $table);
    if (sizeof($module_exports)) {
      // Now load the same objects from the site.
      $site_exports = em_get_site_exportables($table, $module_exports);
      foreach ($site_exports as $name => $export) {
        $func = em_export($table, 'status callback');
        $overridden = $func($export) & EM_STATUS_IN_DATABASE;
        if ($overridden or !$quick) {
          $output[$table] .= "  " . $name . ': ' .
            ($overridden ? 'Overridden' : 'Normal') .
            "\n";
        }
      }
    }
  }
  foreach ($output as $table => $listing) {
    drush_print($table . ':');
    drush_print($listing);
  }
}

function em_revert() {
  if (!$module = em_get_module()) {
    return FALSE;
  }

  $arguments = func_get_args();
  if (empty($arguments)) {
    return drush_set_error(dt('No exportables specified.'));
  }
  $all = FALSE;
  if (in_array('all', $arguments)) {
    $all = TRUE;
  }

  $resetables = array();
  // Find resettables
  $exports = em_export();
  foreach ($exports as $table => $export) {
    $module_exports = em_get_module_exports($module, $table);
    $site_exports = em_get_site_exportables($table, $module_exports);
    $func = $export['status callback'];
    foreach ($site_exports as $name => $object) {
      if ($func($object) & EM_STATUS_IN_DATABASE) {
        $resetables[$table][$name] = $object;
      }
    }
  }

  // Filter to specified.
  foreach ($arguments as $arg) {
    if ($arg == 'all') {
      $reset = $resetables;
      break;
    }
    list($table, $name) = explode(':', $arg, 2);
    if (!$name) {
      return drush_set_error(dt('Malformed export @name.', array('@name' => $arg)));
    } else {
      if ($resetables[$table][$name]) {
        $reset[$table][$name] = $resetables[$table][$name];
      } else {
        return drush_set_error(dt('Unknown exportable or not overridden: @name.', array('@name' => $arg)));
      }
    }
  }

  drush_op('em_revert_exportables', $reset);
  drush_print(dt('Done.'));
}

function em_revert_exportables($exportables) {
  $return = TRUE;
  $exports = em_export();
  foreach ($exportables as $table => $objects) {
    foreach ($objects as $name => $object) {
      if ($exports[$table]['delete callback']) {
        drush_print(dt('Trying to revert @name.', array('@name' => $name)));
        drush_op($exports[$table]['delete callback'], $object);
        // $exports[$table]['delete callback']($object);
      } else {
        drush_set_error(dt('Don\' know how to revert @type, yet. Gonna do something about it, punk?.', array('@type' => $table)));
        $return = FALSE;
      }
    }
  }
  return $return;
}


function em_dump() {
  if (!$module = em_get_module()) {
    return FALSE;
  }

  $arguments = func_get_args();
  if (empty($arguments)) {
    return drush_set_error(dt('No exportables specified.'));
  }

  foreach ($arguments as $arg) {
    list($table, $name) = explode(':', $arg, 2);
    if (!$name) {
      return drush_set_error(dt('Malformed export @name.', array('@name' => $arg)));
    } else {
      $dump[$table][] = $name;
    }
  }

  $reexport = array();
  foreach ($dump as $table => $entries) {
    if ($export = em_export($table)) {
      $site_exportables = em_get_site_exportables($table, array_flip($entries));
      foreach ($entries as $entry) {
        if (!$site_exportables[$entry]) {
          return drush_set_error(dt("Unknown @type '@entry'.",
                                    array('@type' => $table, '@entry' => $entry)));
        } else {
          $reexport[$table][$entry] = $entry;
        }
      }
      // Take API into account. hook_default_page_manager_handlers and
      // hook_default_page_manager_pages uses the same file.
      $all_exportables = em_export();
      foreach ($all_exportables as $other_table => $other_export) {
        if ($export['api'] == $other_export['api'] and !$dump[$other_table]) {
          // Add in other exportables that uses the same API.  We just set it
          // to an empty array, as we add in existing exportables later.
          $reexport[$other_table] = array();
        }
      }
    } else {
      return drush_set_error(dt('Unknown type @type.', array('@type' => $table)));
    }
  }

  // Now, add in the existing ones.
  foreach ($reexport as $table => $entries) {
    $module_exports = em_get_module_exports($module, $table);
    // Override or add.
    $module_exports = array_merge($module_exports,
                                  em_get_site_exportables($table, $entries));
    $reexport[$table] = $module_exports;
  }

  em_write_exportables($module, $reexport);

  drush_op('em_revert_exportables', $reexport);

  drush_print('Done.');
}

function em_remove() {
  if (!$module = em_get_module()) {
    return FALSE;
  }

  // Get arguments.
  $remove = array();
  $arguments = func_get_args();
  if (empty($arguments)) {
    return drush_set_error(dt('No exportables specified.'));
  }

  foreach ($arguments as $arg) {
    list($table, $name) = explode(':', $arg, 2);
    if (!$name) {
      return drush_set_error(dt('Malformed export @name.', array('@name' => $arg)));
    } else {
      $remove[$table][$name] = $name;
    }
  }


  // Get overridden module exports.
  // TODO: don't bother rewriting files, if we're not messing with it.
  $module_exports = em_get_module_exports_all($module);
  foreach ($module_exports as $table => $exports) {
    $site_exports = em_get_site_exportables($table, $exports);
    foreach ($site_exports as $name => $object) {
      if ($remove[$table][$name]) {
        unset($module_exports[$table][$name]);
      }
    }
  }

  // Write files.
  em_write_exportables($module, $module_exports);

}

function em_update() {
  if (!$module = em_get_module()) {
    return FALSE;
  }

  $api_mapping = array();
  $apis_changed = array();
  // Get overridden module exports.
  // TODO: Only rewrite files if anything is changed.
  $module_exports = em_get_module_exports_all($module);
  foreach ($module_exports as $table => $exports) {
    $site_exports = em_get_site_exportables($table, $exports);
    $api = em_export($table, 'api');
    $api_mapping[$api][] = $table;
    foreach ($site_exports as $name => $object) {
      $func = em_export($table, 'status callback');
      if ($func($object) & EM_STATUS_IN_DATABASE) {
        // Replace module by sites.
        $apis_changed[$api] = TRUE;
        $module_exports[$table][$name] = $object;
      }
    }
  }
  $rewrite = array();
  foreach ($apis_changed as $api => $dummy) {
    foreach ($api_mapping[$api] as $table) {
      $rewrite[$table] = $module_exports[$table];
    }
  }
  // Write files.
  em_write_exportables($module, $rewrite);
  drush_print('Updated files');

  drush_op('em_revert_exportables', $rewrite);
}

function em_write_exportables($module, $exportables) {
  $code = array();

  $files = array();
  // Sort tables into api (file) buckets.
  foreach ($exportables as $table => $objects) {
    $files[em_export($table, 'api')][$table] = $objects;
  }

  // Generate new file content.
  foreach ($files as $api => $tables) {
    // Sort tables to keep a stable ordering.
    ksort($tables);
    foreach ($tables as $table => $objects) {
      $export = em_export($table);
      if (function_exists($export['pre export'])) {
        $export['pre export']();
      }
      if (function_exists($export['to hook code callback'])) {
        // Having their own export function means that we can only export the
        // whole shebang. Figure out what that means and ask the user if it's
        // OK.
        // Get the module's list of objects
        // check that there's not anyone of those that's not in the set we're exporting
        $module_exports = em_get_module_exports($module, $table);
        $extra = array();
        foreach ($objects as $name => $object) {
          if (!isset($exportables[$table][$name])) {
            $extra[] = $name;
          }
        }

        if (sizeof($extra) > 0) {
          drush_print(dt('@type defines its own export function, so we have to export all overridden objects.', array('@type' => $table)));
          if (!drush_confirm(dt('This will also export @objects, continue?', array('@objects' => join(', ', $extra))))) {
            return drush_set_error('Aborting.');
          }
        }

        $output = $export['to hook code callback'](array_keys($objects), $module);
        $code[$api] .= $output;
      } else {
        $output = "/**\n";
        $output .= " * Implementation of hook_{$export['default hook']}().\n";
        $output .= " */\n";
        $output .= "function " . $module . "_{$export['default hook']}() {\n";
        foreach ($objects as $object) {
          $output .= $export['export callback']($object, '  '); // if this function does not exist, better to error out than fail silently
          $output .= "  \${$export['identifier']}s['" . check_plain($object->$export['key']) . "'] = \${$export['identifier']};\n\n";
        }
        $output .= " return \${$export['identifier']}s;\n";
        $output .= "}\n";
        $code[$api] .= $output;
      }
    }
  }

  // Write new files.
  // Apparently drush thinks we're in the modules directory, while we're in
  // fact in the site root. Odd.
  $old = getcwd();
  chdir(drush_cwd());
  // TODO: The api hook and the dependencies.
  foreach ($code as $api => $hook_code) {
    $file_code = "<?php\n";
    $file_code .= "// \$Id" . ": $\n\n";
    $file_code .= "/**\n";
    $file_code .= " * @file\n";
    $file_code .= " * Bulk export of $api objects exported by Drush em.\n";
    $file_code .= " */\n\n";
    $file_code .= $hook_code;
    em_write($module . '.' . $api . '.inc', $file_code);
  }
  chdir($old);
  return TRUE;
}


function em_write($filename, $content) {
  // Still write the new file.
  file_put_contents($filename . '.new', $content);
  drush_op('rename', $filename, $filename . '.em~');
  drush_op('rename', $filename . '.new', $filename);
}

/**
 * TODO: Refactor.
 */
function em_list() {
  // Find out what exportables that's available.
  foreach (array_keys(em_export()) as $table) {
    $site_exports = em_get_site_exportables($table);
    foreach ($site_exports as $name => $export) {
      $func = em_export($table, 'status callback');
      if ($func($export) == EM_STATUS_IN_DATABASE) {
        $output[$table] .= "  " . $table . ':'. $name . "\n";
      }
    }
  }
  foreach ($output as $table => $listing) {
    drush_print($table . ':');
    drush_print($listing);
  }
}

/**
 * Get all exportables from module.
 */
function em_get_module_exports_all($module) {
  $all_exportables = array();
  foreach (array_keys(em_export()) as $table) {
    $exportables = em_get_module_exports($module, $table);
    if ($exportables) {
      $all_exportables[$table] = $exportables;
    }
  }
  return $all_exportables;
}

/**
 * Get module defined exportables.
 */
function em_get_module_exports($module, $table) {
  $module_exports = array();
  // Apparently drush thinks we're in the modules directory, while we're in
  // fact in the site root. Odd.
  $export = em_export($table);
  //$old = getcwd();
  //chdir(drush_cwd());
  $hook_file = drush_cwd() . "/" . $module . '.' . $export['api'] . '.inc';
  if (file_exists($hook_file)) {
    require_once($hook_file);
    $hook = $module . "_" . $export['default hook'];
    if (function_exists($hook)) {
      $module_exports = $hook();
    }
  }
  //chdir($old);
  return $module_exports;
}

/**
 * Returns exportables from database.
 */
function em_get_site_exportables($table, $exportables = NULL) {
  $export = em_export($table);
  if (is_null($exportables) and function_exists($export['list callback'])) {
    $exportables = $export['list callback']();
  }
  $args = $export['load arguments'];
  $args[] = array_keys($exportables);
  $site_exports = call_user_func_array($export['load callback'], $args);
  return $site_exports;
}

function em_get_module() {
  // Find a module.
  // Apparently drush thinks we're in the modules directory, while we're in
  // fact in the site root. Odd.
  $old = getcwd();
  chdir(drush_cwd());
  $modules = glob('*.module');
  chdir($old);
  $candidates = array();
  foreach ($modules as $name) {
    $candidates[] = basename($name, '.module');
  }

  if (sizeof($candidates) == 1) {
    return $candidates[0];
  } else {
    // TODO: if more than one, list and provide argument.
    return drush_set_error(dt('No module.'));
  }
}

/**
 * Export a field.
 *
 * Stolen from views.
 */
function em_var_export($var, $prefix = '', $init = TRUE) {
  if (is_array($var)) {
    if (empty($var)) {
      $output = 'array()';
    }
    else {
      $output = "array(\n";
      foreach ($var as $key => $value) {
        $output .= "  '$key' => " . views_var_export($value, '  ', FALSE) . ",\n";
      }
      $output .= ')';
    }
  }
  else if (is_bool($var)) {
    $output = $var ? 'TRUE' : 'FALSE';
  }
  else if (is_string($var) && strpos($var, "\n") !== FALSE) {
    // Replace line breaks in strings with a token for replacement
    // at the very end. This protects multi-line strings from
    // unintentional indentation.
    $var = str_replace("\n", "***BREAK***", $var);
    $output = var_export($var, TRUE);
  }
  else {
    $output = var_export($var, TRUE);
  }

  if ($prefix) {
    $output = str_replace("\n", "\n$prefix", $output);
  }

  if ($init) {
    $output = str_replace("***BREAK***", "\n", $output);
  }

  return $output;
}
