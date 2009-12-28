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
function em_drush_command() {
  $items['em status'] = array(
    // We're skipping the customary drush_ prefix, as it increases the chance
    // of colliding with a hook.
    'callback' => 'em_status',
    // 'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_SITE,
  );

  $items['em revert'] = array(
    'callback' => 'em_revert',
    'arguments' => array(
      'exportables' => 'Space delimited list of exportables to revert, or \'all\' to revert all module defined exportables.'
    ),
  );
  
  $items['em dump'] = array(
    'callback' => 'em_dump',
    'arguments' => array(
      'exportables' => 'Space delimited list of exportables to add.'
    ),
  );

  $items['em remove'] = array(
    'callback' => 'em_remove',
    'arguments' => array(
      'exportables' => 'Space delimited list of exportables to remove.'
    ),
  );

  $items['em update'] = array(
    'callback' => 'em_update',
  );
  
  $items['em list'] = array(
    'callback' => 'em_list',
  );
  return $items;
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
  static $defaults = array(
    'page_manager_pages' => array(
      'pre export' => 'page_manager_get_tasks',
      'delete hook' => 'em_delete_page_manager_pages',
    ),
    'page_manager_handlers' => array(
      'delete hook' => 'page_manager_delete_task_handler',
    ),
    'panels_mini' => array(
      'delete hook' => 'panels_mini_delete',
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
      );
    }

    // Find out what ctools exportables that's available.
    ctools_include('export');
    $schemas = ctools_export_get_schemas(TRUE);
  
    foreach ($schemas as $etable => $schema) {
      if (function_exists($schema['export']['list callback']) and $schema['export']['api']) {
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
        );
        if ($defaults[$etable]) {
          $export_types[$etable] += $defaults[$etable];
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


function em_status() {
  if (!$module = em_get_module()) {
    return FALSE;
  }

  // Now, find out what exportables that's available.
  $export_schemas = em_export();

  foreach ($export_schemas as $table => $export) {
    $site_exports = array();
    $module_exports = em_get_module_exports($module, $table);
    if (sizeof($module_exports)) {
      // Now load the same objects from the site.
      $site_exports = em_get_site_exportables($table, $module_exports);
      foreach ($site_exports as $name => $export) {
        // TODO: Generalise normal/overridden detection.
        $output[$table] .= "  " . $name . ': ' .
          ($export instanceof view ?
           ($export->type == t('Overridden') ? 'Overridden' : 'Normal') :
           ($export->export_type == EXPORT_IN_CODE ? 'Normal' : 'Overridden')) . "\n";
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
    foreach ($site_exports as $name => $object) {
      if ($object->export_type & EXPORT_IN_DATABASE) {
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
        return drush_set_error(dt('Unknown exportable @name.', array('@name' => $arg)));      
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
      if ($exports[$table]['delete hook']) {
        $exports[$table]['delete hook']($object);
      } else {
        drush_set_error(dt('Don\' know how to revert @type, yet. Gonna do something about it, punk?.', array('@type' => $table)));
        $return = FALSE;
      }
      drush_print(dt('Reverted @name.', array('@name' => $name)));
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
    if (em_export($table)) {
      $site_exportables = em_get_site_exportables($table);
      foreach ($entries as $entry) {
        if (!$site_exportables[$entry]) {
          return drush_set_error(dt("Unknown @type '@entry'.",
                                    array('@type' => $table, '@entry' => $entry)));
        } else {
          $reexport[$table][$entry] = $entry;
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

  // Get overridden module exports.
  $module_exports = em_get_module_exports_all($module);
  foreach ($module_exports as $table => $exports) {
    $site_exports = em_get_site_exportables($table, $exports);
    foreach ($site_exports as $name => $object) {
      // TODO: hook(?) for non-ctools tables
      if (!($object->export_type ^ EXPORT_IN_DATABASE)) {
        unset($module_exports[$table][$name]);
      }
    }
  }

  // Write files.
  em_write_exportables($module, $module_exports);

  drush_op('em_revert_exportables', $module_exports);
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
        $output .= " * Implementation of hook_{$export['default hook']}()\n";
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
    $file_code .= " * Bulk export of $api objects exported by Drush ce.\n";
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
      // TODO: roll into em_export
      if ($export->export_type == EXPORT_IN_DATABASE) {
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
