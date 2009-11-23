<?php

/*
em status:
list hvilke der er overridden

em reset:
slet overrideen

em dump:
exporter navngivne til fil, bevar eksisterende (for så vidt muligt)

em del:
slet given export

em update:
reexport alle overridden

em list:
vis alle exportables vi kan klare

em diff:
vis forskelle (hvis det er muligt)

 */
function em_drush_command() {
  $items['em status'] = array(
    'callback' => 'drush_em_status',
    // 'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_SITE,
  );

  $items['em dump'] = array(
    'callback' => 'drush_em_dump',
    'arguments' => array(
      'exportables' => 'Space delimited list of exportables to add.'
    ),
  );

  $items['em list'] = array(
    'callback' => 'drush_em_list',
  );
  return $items;
}

function drush_em_status() {
  if (!$module = drush_em_get_module()) {
    return FALSE;
  }

  // Now, find out what exportables that's available.
  $export_schemas = drush_em_get_schemas();

  foreach ($export_schemas as $table => $export) {
    $site_exports = array();
    $module_exports = drush_em_get_module_exports($module, $export);
    if (sizeof($module_exports)) {
      // Now load the same objects from the site.
      $site_exports = drush_em_get_exportables($table, $export, $module_exports);
      foreach ($site_exports as $name => $export) {
        $output[$table] .= "  " . $name . ': ' . ($export->export_type == EXPORT_IN_CODE ? 'Normal' : 'Overridden') . "\n";
      }
    }
  }
  foreach ($output as $table => $listing) {
    drush_print($table . ':');
    drush_print($listing);
  }
}

function drush_em_dump() {
  if (!$module = drush_em_get_module()) {
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
  $exports = drush_em_get_schemas();
  foreach ($dump as $table => $entries) {
    if ($exports[$table]) {
      $site_exportables = drush_em_get_exportables($table, $exports[$table]);
      foreach ($entries as $entry) {
        if (!$site_exportables[$entry]) {
          return drush_set_error(dt("Unknown @type '@entry'.", array('@type' => $table, '@entry' => $entry)));
        } else {
          $reexport[$exports[$table]['api']['api']][$table][$entry] = $entry;
        }
        
      }
    } else {
      return drush_set_error(dt('Unknown type @type.', array('@type' => $table)));      
    }
  }

  $out = array();
  // Now, add in the existing ones.
  foreach ($reexport as $api => $tables) {
    foreach ($tables as $table => $entries) {
      $module_exports = drush_em_get_module_exports($module, $exports[$table]);
      // Override or add.
      $module_exports = array_merge($module_exports,
                                    drush_em_get_exportables($table, $exports[$table], $entries));
      $reexport[$api][$table] = $module_exports;
    }
  }

  $code = array();
  // Generate new file content.
  foreach ($reexport as $api => $tables) {
    foreach ($tables as $table => $objects) {
      $export = $exports[$table];
      if (function_exists($export['to hook code callback'])) {
        drpr(array_keys($objects));
        // Having their own export function means that we can only export the
        // whole shebang. Figure out what that means and ask the user if it's
        // OK.
        $extra = array();
        foreach ($objects as $name => $object) {
          if (!in_array($name, $dump[$table])) {
            $extra[] = $name;
          }
        }

        drush_print(dt('@type defines its own export function, so we have to export all overridden objects.', array('@type' => $table)));
        if (!drush_confirm(dt('This will also export @objects, continue?', array('@objects' => join(', ', $extra))))) {
          return drush_set_error('Aborting.');
        }

        // TODO: For some reason, the right task include might not be loaded, so we tickle it here.
        if ($table == 'page_manager_pages') {
          page_manager_get_tasks();
        }

        $output = $export['to hook code callback'](array_keys($objects), $module);
        $code[$api] = $output;
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
        $code[$api] = $output;
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
    $filename = $module . '.' . $api . '.inc';
    file_put_contents($filename . '.new', $file_code);
    rename($filename, $filename . '.em~');
    rename($filename . '.new', $filename);
  }
  chdir($old);
  drush_print('Done.');
}

function drush_em_list() {
  // Find out what exportables that's available.
  $export_schemas = drush_em_get_schemas();

  foreach ($export_schemas as $table => $export) {
    $site_exports = drush_em_get_exportables($table, $export);
    foreach ($site_exports as $name => $export) {
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


function drush_em_get_module_exports($module, $export) {
  // Apparently drush thinks we're in the modules directory, while we're in
  // fact in the site root. Odd.
  $old = getcwd();
  chdir(drush_cwd());
  $hook_file = $module . '.' . $export['api']['api'] . '.inc';
  if (file_exists($hook_file)) {
    require_once($hook_file);
    $hook = $module . "_" . $export['default hook'];
    if (function_exists($hook)) {
      $module_exports = $hook();
    }
  }
  chdir($old);
  return $module_exports;
}


function drush_em_get_schemas() {
  // Now, find out what exportables that's available.
  ctools_include('export');
  $schemas = ctools_export_get_schemas(TRUE);
  $export_schemas = array();
  
  foreach ($schemas as $table => $schema) {
    if (function_exists($schema['export']['list callback']) and $schema['export']['api']) {
      $export_schemas[$table] = $schema['export'];
    }
  }
  return $export_schemas;
}

function drush_em_get_exportables($table, $export, $exportables = NULL) {
  if (is_null($exportables) and function_exists($export['list callback'])) {
    $exportables = $export['list callback']();
  }
  $site_exports = ctools_export_load_object($table, 'names', array_keys($exportables));
  return $site_exports;
}


function drush_em_get_module() {
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