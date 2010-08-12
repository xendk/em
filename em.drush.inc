<?php

// $Id$

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
  // Some things depend on drush version.
  $drush_three_plus = defined('DRUSH_VERSION');

  $items['em-status'] = array(
    'description' => 'Show exportable status of current module, or all modules.',
    // Work around drush 2 thinking that drush_em_status is a hook on the status command.
    'callback' => 'drush_em_em_status',
    'options' => array(
      '-o' => 'Only show overridden.',
      '-a' => 'Show all modules',
    ),
    'examples' => array(
      'em status' => 'Show status.',
    ),
    // 'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_SITE,
  );

  $items['em-revert'] = array(
    'description' => 'Revert an exportable.',
    'arguments' => array(
      'exportables' => 'Space delimited list of exportables to revert, or \'all\' to revert all module defined exportables.'
    ),
    'examples' => array(
      'em revert views:frontpage' => 'Revert the view "frontpage".',
    ),
    'options' => array(
      '-e' => "Don't revert, just show what would have been done.",
    ),
  );

  $items['em-dump'] = array(
    'description' => 'Dump or update exportables to the current module.',
    'arguments' => array(
      'exportables' => 'Space delimited list of exportables to add/update.'
    ),
    'examples' => array(
      'em dump views:frontpage' => 'Dump the view "frontpage" to the relevant file.',
    ),
    'options' => array(
      '-e' => "Don't revert exportables.",
    ),
  );

  $items['em-remove'] = array(
    'description' => 'Remove an exportable from the current module.',
    'arguments' => array(
      'exportables' => 'Space delimited list of exportables to remove.'
    ),
    'examples' => array(
      'em remove views:frontpage' => 'Remove the "frontpage" view from the relevant file.',
    ),
  );

  $items['em-update'] = array(
    'description' => 'Update module exportables from site.',
    'options' => array(
      '-e' => "Don't revert exportables.",
    ),
  );

  $items['em-list'] = array(
    'description' => 'List site exportables.',
  );

  if ($drush_three_plus) {
    $items['em-diff'] = array(
      'description' => 'Show differences between code and database.',
      'options' => array(
        '-t' => 'Terse mode. Only show differing lines.',
      ),
    );
  }

  $items['em-debug'] = array(
    'description' => 'Debug...',
  );

  if (!$drush_three_plus) {
    // Define the callbacks for drush 2.
    foreach ($items as $name => &$item) {
      if (!isset($item['callback'])) {
        $item['callback'] = 'drush_' .  str_replace("-", "_", $name);
      }
    }
  }

  return $items;
}

/**
 * Implementation of hook_drush_help().
 */
function em_drush_help($section) {
  switch ($section) {
    case 'drush:em-status':
      return dt('Lists all exportables defined by the current module, and whether they are overridden or not.');
    case 'drush:em-revert':
      return dt('Reverts the given exportables to their code defined state.');
    case 'drush:em-dump':
      return dt('Dump exportables to the current module. You might need to add a function like hook_ctools_api to your module file.');
    case 'drush:em-remove':
      return dt('Remove an exportable from the current module.');
    case 'drush:em-update':
      return dt('Updates all overridden exportables defined by the current module with the settings from the site, and reverts them afterwards.');
    case 'drush:em-list':
      return dt('Lists all site exportables that is not defined in code.');
    case 'drush:em-diff':
      return dt('Show differences between site and code for overridden exportables.');
  }
}

function em_match($string, $haystack, $type) {
  if (in_array($string, $haystack)) {
    return array($string);
  }
  $candidates = array();
  if (strpos($string, '*') !== FALSE) {
    // Pattern matching.
    $string = strtr($string, array('*' => '.*'));
    foreach ($haystack as $hay) {
      if (preg_match('{^' . $string . '$}', $hay)) {
        $candidates[] = $hay;
      }
    }
  }
  else {
    // Substring matching.
    foreach ($haystack as $hay) {
      if (strpos($hay, $string) !== FALSE) {
        $candidates[] = $hay;
      }
    }
    if (sizeof($candidates) > 1) {
      return drush_set_error(dt('Ambiguous @type @abbrev. Possible exports: @exports',
          array('@type' => $type, '@abbrev' => $string, '@exports' => join(', ', $candidates))));
    }
  }
  return $candidates;
}

function em_filter_exportables($patterns, &$exportables, &$matched) {
  $matched_exports = array();
  if ($patterns[0] == 'all') {
    $matched_exports = $exportables;
    $exportables = array();
    return $matched_exports;
  }
  // Filter to specified.
  foreach ($patterns as $pattern) {
    list($table_pattern, $name_pattern) = explode(':', $pattern, 2);
    if (!$name_pattern) {
      return drush_set_error(dt('Malformed pattern @pattern.', array('@pattern' => $pattern)));
    } else {
      $table_candidates = em_match($table_pattern, array_keys($exportables), 'export');
      if ($table_candidates === FALSE) {
        return FALSE;
      }
      if ($table_candidates) {
        if (sizeof($table_candidates) > 1) {
          $matched = TRUE;
        }
        foreach ($table_candidates as $table_candidate) {
          $export_candidates = em_match($name_pattern, array_keys($exportables[$table_candidate]), $table_candidate);
          // Only error out if the table wasn't matched by pattern.
          if ($export_candidates === FALSE and sizeof($table_candidates) == 1) {
            return FALSE;
          }
          elseif ($export_candidates) {
            if (!$matched_exports[$table_candidate]) {
              $matched_exports[$table_candidate] = array();
            }
            if (sizeof($export_candidates) > 1) {
              $matched = TRUE;
            }
            foreach ($export_candidates as $export_candidate) {
              $matched_exports[$table_candidate][$export_candidate] = $exportables[$table_candidate][$export_candidate];
              unset($exportables[$table_candidate][$export_candidate]);
            }
          }
          elseif (sizeof($table_candidates) == 1) {
            return drush_set_error(dt("No @type '@entry'.",
                array('@type' => $table_candidate, '@entry' => $name_pattern)));
          }
        }
      }
      else {
        return drush_set_error(dt('No export @type.', array('@type' => $table_pattern)));
      }
    }
  }

  return $matched_exports;
}

function em_exportables_apis($exportables) {
  $apis = array();
  foreach ($exportables as $table => $dummy) {
    $apis[] = em_export($table, 'api');
  }
  return $apis;
}

function em_filter_exportables_api($apis, &$exportables) {
  $removed = array();
  foreach ($exportables as $table => $dummy) {
    if (in_array(em_export($table, 'api'), $apis)) {
      $removed[$table] = $exportables[$table];
      unset($exportables[$table]);
    }
  }
  return $removed;
}

function drush_em_debug($string) {
  $arguments = func_get_args();
  if (empty($arguments)) {
    return drush_set_error(dt('No exportables specified.'));
  }

  foreach ($arguments as $arg) {
    list($table_abbrev, $name) = explode(':', $arg, 2);
    if (!$name) {
      return drush_set_error(dt('Malformed export @name.', array('@name' => $arg)));
    }
    $candidates = em_match($table_abbrev, array_keys(em_export()), 'export');
    if ($candidates === FALSE) {
      return FALSE;
    }
    if ($candidates) {
      foreach ($candidates as $table) {
        $dump[$table][] = $name;
      }
    }
    else {
      return drush_set_error(dt('Unknown export @type.', array('@type' => $table_abbrev)));
    }
  }

  drush_print_r($dump);
  // drush_print_r(em_export());
}

/**
 * Return information about known exportable types.
 */
function em_export($table = NULL, $key = NULL) {
  static $export_types = NULL;

  if (!is_array($export_types)) {
    // Load include files that gaffas em together with assorted modules that
    // doesn't support it directly.
    $include_dir = dirname(__FILE__) . '/modules/';
    foreach (module_list() as $module) {
      $inc_file = $include_dir . $module . '.inc';
      if (file_exists($inc_file)) {
        require_once($inc_file);
      }
    }

    // Collect information.
    $export_types = module_invoke_all('em_exportables');
    drupal_alter('em_exportables', $export_types);
  }

  if ($key) {
    return $export_types[$table][$key];
  } elseif ($table) {
    return $export_types[$table];
  }
  return $export_types;
}

function drush_em_em_status() {
  $modules = array();
  if (drush_get_option('a', FALSE)) {
    $modules = module_list();
  }
  elseif ($module = em_get_module()) {
    $modules[] = $module;
  }

  if (empty($modules)) {
    return FALSE;
  }

  // Now, find out what exportables that's available.
  $export_schemas = em_export();
  $quick = drush_get_option('o', FALSE);
  foreach ($modules as $module) {
    $output = array();
    foreach ($export_schemas as $table => $export) {
      $site_exports = array();
      $module_exports = em_get_module_exports($module, $table);
      if (sizeof($module_exports)) {
        // Now load the same objects from the site.
        $site_exports = em_get_site_exportables($table, $module_exports);
        foreach ($site_exports as $name => $export) {
          if (($overridden = em_is_in_database($table, $export)) or !$quick) {
            $output[$table] .= "  " . $name . ': ' .
              ($overridden ? 'Overridden' : 'Normal') .
              "\n";
          }
        }
      }
    }
    if (sizeof($modules) > 1 and sizeof($output)) {
      drush_print('--- ' . $module . ' ---');
    }
    foreach ($output as $table => $listing) {
      drush_print($table . ':');
      drush_print($listing);
    }
  }
}

function drush_em_diff() {
  if ($module = em_get_module()) {
    $modules[] = $module;
  } else {
    return FALSE;
  }

  $diff_opts = '';
  if (drush_get_option('t', FALSE)) {
    $diff_opts .= '--suppress-common-lines';
  }

  $export_schemas = em_export();
  foreach ($modules as $module) {
    $output = array();
    foreach ($export_schemas as $table => $export) {
      $site_exports = array();
      $module_exports = em_get_module_exports($module, $table);
      if (sizeof($module_exports)) {
        // Now load the same objects from the site.
        $site_exports = em_get_site_exportables($table, $module_exports);
        foreach ($site_exports as $name => $database_export) {
          if (em_is_in_database($table, $database_export)) {

            if (function_exists($export['export callback'])) {
              drush_print($table . ' ' . $name . ':');
              $args = isset($export['export arguments']) ? $export['export arguments'] : array();
              $args[] = $module_exports[$name];
              $args[] = '';
              $code = call_user_func_array($export['export callback'], $args);
              $args = isset($export['export arguments']) ? $export['export arguments'] : array();
              $args[] = $database_export;
              $args[] = '';
              $database  = call_user_func_array($export['export callback'], $args);
              $database = $export['export callback']($database_export, '');
              drush_print(em_diff($code, $database, $diff_opts));
              drush_print();
            }
            else {
              drush_print(dt("Cannot diff @table, no export hook.", array('@table' => $table)));
            }
          }
        }
      }
    }
  }
}

function em_diff($a, $b, $diff_opts) {
  // drush_save_data_to_temp_file() can't handle 2 concurrent files, so we do
  // it this way.
  $a_file = drush_tempnam('em-a');
  file_put_contents($a_file, $a);
  $b_file = drush_tempnam('em-b');
  file_put_contents($b_file, $b);
  //drush_print_r(array($a_file, $b_file));
  //drush_print(file_exists($a_file) ? 'yes' : 'no');
  return shell_exec("/usr/bin/diff -y $diff_opts $a_file $b_file");
}

function drush_em_revert() {
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
      if (em_is_in_database($table, $object)) {
        $resetables[$table][$name] = $object;
      }
    }
  }

  // Filter to specified.
  $multiple = FALSE;
  $reset = em_filter_exportables($arguments, $resetables, $multiple);
  if ($reset === FALSE) {
    return;
  }

  drush_print("Will reset:");
  foreach ($reset as $table => $entries) {
    $names = array();
    foreach ($entries as $name => $dummy) {
      $names[] = $name;
    }
    drush_print($table . ':' . join(', ', $names));
  }
  if ($multiple) {
    if (!drush_confirm("Are you sure you want to do this?")) {
      return;
    }
  }

  drush_op('em_revert_exportables', $reset);
  drush_print(dt('Done.'));
}

function em_revert_exportables($exportables) {
  $return = TRUE;
  $exports = em_export();
  $dry = drush_get_option('e', FALSE);

  foreach ($exportables as $table => $objects) {
    foreach ($objects as $name => $object) {
      if ($exports[$table]['delete callback']) {
        if ($dry) {
          drush_print(dt('Would try to revert @name.', array('@name' => $name)));
        }
        else {
          drush_print(dt('Trying to revert @name.', array('@name' => $name)));
          $args = isset($exports[$table]['delete arguments']) ? $exports[$table]['delete arguments'] : array();
          array_unshift($args, $exports[$table]['delete callback']);
          $args[] = $object;
          $output .= call_user_func_array('drush_op', $args);
        }
      } else {
        drush_set_error(dt('Don\'t know how to revert @type, yet. Gonna do something about it, punk?.', array('@type' => $table)));
        $return = FALSE;
      }
    }
  }
  return $return;
}

function drush_em_dump() {
  if (!$module = em_get_module()) {
    return FALSE;
  }

  $arguments = func_get_args();
  if (empty($arguments)) {
    return drush_set_error(dt('No exportables specified.'));
  }

  $site_exportables = array();
  foreach (em_export() as $table => $export) {
    $site_exportables[$table] = em_get_site_exportables($table);
  }
  $reexport = em_filter_exportables($arguments, $site_exportables, $dummy);
  if ($reexport === FALSE) {
    return;
  }

  // Now, add in the existing ones.
  foreach ($reexport as $table => $entries) {
    $module_exports = em_get_module_exports($module, $table);
    // Override or add.
    $revert[$table] = em_get_site_exportables($table, $entries);
    $new_module_exports = array_merge($module_exports, $revert[$table]);
    $reexport[$table] = $new_module_exports;
  }

  // Take API into account. hook_default_page_manager_handlers and
  // hook_default_page_manager_pages uses the same file.
  $all_module_exports = em_get_module_exports_all($module);
  $shared_apis = em_filter_exportables_api(em_exportables_apis($reexport), $all_module_exports);
  foreach ($shared_apis as $table => $entries) {
    if (!isset($reexport[$table])) {
      $reexport[$table] = array();
    }
    $reexport[$table] = array_merge($entries, $reexport[$table]);
  }

  em_write_exportables($module, $reexport);

  drush_op('em_revert_exportables', $revert);

  drush_print('Done.');
}

function drush_em_remove() {
  if (!$module = em_get_module()) {
    return FALSE;
  }

  // Get arguments.
  $remove = array();
  $arguments = func_get_args();
  if (empty($arguments)) {
    return drush_set_error(dt('No exportables specified.'));
  }

  // Get module exports.
  $module_exports = em_get_module_exports_all($module);

  $exports = em_export();
  $removed = em_filter_exportables($arguments, $module_exports, $dummy);
  if ($removed === FALSE) {
    return;
  }

  // Filter down to exports sharing api with the removed. The removed
  // has been removed from $module_exports.
  $new_module_exports = em_filter_exportables_api(em_exportables_apis($removed), $module_exports);

  // Write files.
  em_write_exportables($module, $new_module_exports);

}

function drush_em_update() {
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
    if ($site_exports) {
      foreach ($site_exports as $name => $object) {
        if (em_is_in_database($table, $object)) {
          // Replace module by sites.
          $apis_changed[$api] = TRUE;
          $module_exports[$table][$name] = $object;
        }
      }
    }
    else {
      // If exports are empty, the api exists, but returns nothing. Flag it for deletion.
      $apis_changed[$api] = TRUE;
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
      $code[$api] .= '';
      if (function_exists($export['to hook code callback'])) {
        if (sizeof($objects)) {
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
        }
      } else {
        if (sizeof($objects)) {
          $output = "/**\n";
          $output .= " * Implementation of hook_{$export['default hook']}().\n";
          $output .= " */\n";
          $output .= "function " . $module . "_{$export['default hook']}() {\n";
          foreach ($objects as $object) {
            $args = isset($export['export arguments']) ? $export['export arguments'] : array();
            $args[] = $object;
            $args[] = '  ';
            $output .= call_user_func_array($export['export callback'], $args);

            $output .= "  \${$export['identifier']}s['" . check_plain($object->$export['key']) . "'] = \${$export['identifier']};\n\n";
          }
          $output .= " return \${$export['identifier']}s;\n";
          $output .= "}\n";
          $code[$api] .= $output;
        }
      }
    }
  }

  // Write new files.  We change to the current dir of the invocation, to
  // avoid odd behaviour in case of the same module being present at different
  // paths.
  $old = getcwd();

  chdir(drush_cwd());
  // TODO: The api hook and the dependencies.
  foreach ($code as $api => $hook_code) {
    if ($hook_code) {
      $file_code = "<?php\n";
      $file_code .= "// \$Id" . "$\n\n";
      $file_code .= "/**\n";
      $file_code .= " * @file\n";
      $file_code .= " * Bulk export of $api objects exported by Drush em.\n";
      $file_code .= " */\n\n";
      $file_code .= $hook_code;
      em_write($module . '.' . $api . '.inc', $file_code);
    }
    else {
      if (file_exists($module . '.' . $api . '.inc')) {
        drush_op('unlink', $module . '.' . $api . '.inc');
      }
    }
  }
  chdir($old);
  return TRUE;
}

function em_write($filename, $content) {
  // Still write the new file.
  file_put_contents($filename . '.new', $content);
  if (file_exists($filename)) {
    drush_op('rename', $filename, $filename . '.em~');
  }
  drush_op('rename', $filename . '.new', $filename);
}

function drush_em_list() {
  // Find out what exportables that's available.
  $output = array();
  foreach (array_keys(em_export()) as $table) {
    $site_exports = em_get_site_exportables($table);
    foreach ($site_exports as $name => $export) {
      // Only exportables that's only in database.
      if (em_is_overridden($table, $export) == EM_STATUS_IN_DATABASE) {
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
    else {
      $all_exportables[$table] = array();
    }
  }
  return $all_exportables;
}

/**
 * Get module defined exportables.
 */
function em_get_module_exports($module, $table) {
  $module_exports = array();
  $export = em_export($table);
  $hook_file = drupal_get_path('module', $module) . '/' . $module . '.' . $export['api'] . '.inc';
  if (file_exists($hook_file)) {
    require_once($hook_file);
    $hook = $module . "_" . $export['default hook'];
    if (function_exists($hook)) {
      $module_exports = $hook();
    }
  }
  return $module_exports;
}

/**
 * Returns exportables from database.
 */
function em_get_site_exportables($table, $exportables = NULL) {
  $export = em_export($table);
  if (is_null($exportables) && function_exists($export['list callback'])) {
    $args = isset($export['list arguments']) ? $export['list arguments'] : array();
    $exportables = call_user_func_array($export['list callback'], $args);
  }
  $args = isset($export['load arguments']) ? $export['load arguments'] : array();
  if (sizeof($exportables)) {
    $args[] = array_keys($exportables);
    $site_exports = call_user_func_array($export['load callback'], $args);
  }
  else {
    // No exportables found.
    $site_exports = array();
  }
  return $site_exports;
}

function em_get_module($optional = FALSE) {
  // Find a module.
  $modules = glob(drush_cwd() . '/*.module');
  $candidates = array();
  foreach ($modules as $name) {
    $candidates[] = basename($name, '.module');
  }

  if (sizeof($candidates) == 1) {
    return $candidates[0];
  } elseif (!$optional) {
    // TODO: if more than one, list and provide argument.
    return drush_set_error(dt('No module.'));
  }
}

/**
 * Export an object.
 *
 * Stolen from views/CTools.
 */
function em_var_export($var, $prefix = '', $init = TRUE) {
  if (is_array($var)) {
    if (empty($var)) {
      $output = 'array()';
    }
    else {
      $output = "array(\n";
      foreach ($var as $key => $value) {
        $output .= "  '$key' => " . em_var_export($value, '  ', FALSE) . ",\n";
      }
      $output .= ')';
    }
  }
  else if (is_object($var) && get_class($var) === 'stdClass') {
    // var_export() will export stdClass objects using an undefined
    // magic method __set_state() leaving the export broken. This
    // workaround avoids this by casting the object as an array for
    // export and casting it back to an object when evaluated.
    $output .= '(object) ' . ctools_var_export((array) $var);
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

function em_is_overridden($table, $object) {
  $func = em_export($table, 'status callback');
  if (function_exists($func)) {
    return $func($object);
  }
  else {
    return 0;
  }
}

/**
 * Returns whether an export is defined in the site database.
 */
function em_is_in_database($table, $object) {
  return em_is_overridden($table, $object) & EM_STATUS_IN_DATABASE;
}

/**
 * Returns whether an export is defined in the site code.
 */
function em_is_in_code($table, $object) {
  return em_is_overridden($table, $object) & EM_STATUS_IN_CODE;
}
