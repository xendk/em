<?php

/*
status:
list hvilke der er overridden

reset:
få defaults listen, check om nogen er overridden, hvis, generer ny default fil

dump:
exporter navngivne til fil, bevar eksisterende

list:
vis alle exportables vi kan klare

 */
function exporter_drush_command() {
  $items['exporter test'] = array(
    'callback' => 'exporter_exporter_test',
    // 'arguments' => array('domain.com' => dt('The domain of the site to install.')),
    // 'description' => dt('Provision a new site using the provided data.'),
    // 'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT
  );

  $items['ce status'] = array(
    'callback' => 'exporter_ce_status',
    // 'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_SITE,
  );

  $items['ce dump'] = array(
    'callback' => 'drush_ce_dump',
    'arguments' => array(
      'exportables' => 'Space delimited list of exportables to add.'
    ),
  );

  $items['ce list'] = array(
    'callback' => 'drush_ce_list',
  );
  return $items;
}

function exporter_ce_status() {
  if (!$module = drush_ce_get_module()) {
    return FALSE;
  }

  // Now, find out what exportables that's available.
  $export_schemas = drush_ce_get_schemas();

  foreach ($export_schemas as $table => $export) {
    $site_exports = array();
    $module_exports = drush_ce_get_module_exports($module, $export);
    if (sizeof($module_exports)) {
      // Now load the same objects from the site.
      $site_exports = drush_ce_get_exportables($table, $export, $module_exports);
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

function drush_ce_dump() {
  if (!$module = drush_ce_get_module()) {
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
  $exports = drush_ce_get_schemas();
  foreach ($dump as $table => $entries) {
    if ($exports[$table]) {
      $site_exportables = drush_ce_get_exportables($table, $exports[$table]);
      foreach ($entries as $entry) {
        if (!$site_exportables[$entry]) {
          return drush_set_error(dt("Unknown @type '@entry'.", array('@type' => $table, '@entry' => $entry)));
        } else {
          $reexport[$exports[$table]['api']['api']][$table][] = $entry;
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
      $module_exports = drush_ce_get_module_exports($module, $exports[$table]);
      // Override or add.
      $module_exports = array_merge($module_exports,
                                    drush_ce_get_exportables($table, $exports[$table], $entries));
      $reexport[$api][$table] = $module_exports;
    }
  }

  $code = array();
  // Generate new file content.
  foreach ($reexport as $api => $tables) {
    foreach ($tables as $table => $objects) {
      $export = $exports[$table];
      if (function_exists($export['to hook code callback'])) {
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
  drpr($code);
}

function drush_ce_list() {
  // Find out what exportables that's available.
  $export_schemas = drush_ce_get_schemas();

  foreach ($export_schemas as $table => $export) {
    $site_exports = drush_ce_get_exportables($table, $export);
    foreach ($site_exports as $name => $export) {
      if ($export->export_type == EXPORT_IN_DATABASE) {
        $output[$table] .= "  " . $name . "\n";
      }
    }
  }
  foreach ($output as $table => $listing) {
    drush_print($table . ':');
    drush_print($listing);
  }  
}


function drush_ce_get_module_exports($module, $export) {
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


function drush_ce_get_schemas() {
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

function drush_ce_get_exportables($table, $export, $exportables = NULL) {
  if (is_null($exportables) and function_exists($export['list callback'])) {
    $exportables = $export['list callback']();
  }
  $site_exports = ctools_export_load_object($table, 'names', array_keys($exportables));
  return $site_exports;
}


function drush_ce_get_module() {
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


function exporter_find_module() {
  // Apparently drush thinks we're in the modules directory, while we're in
  // fact in the site root. Odd.
  $old = getcwd();
  chdir(drush_cwd());
  $modules = glob('*.module');
  chdir($old);
  $res = array();
  foreach ($modules as $name) {
    $res[] = basename($name, '.module');
  }
  return $res;
}

function drpr($x) {
  drush_print_r($x);
  drush_print('');
}


function exporter_exporter_test() {
  ctools_include('export');
  $schemas = ctools_export_get_schemas(TRUE);
  $exportables = $export_tables = array();

  foreach ($schemas as $table => $schema) {
    if (function_exists($schema['export']['list callback'])) {
      $exportables[$table] = $schema['export']['list callback']();
      $export_tables[$table] = $schema['module'];
    }
  }

  // drush_print(print_r($exportables, TRUE));
  // drush_print(print_r($export_tables, TRUE));

  
  $code = array();
  $module = 'foo';

  foreach ($exportables as $table => $names) {
    $names = array_keys(array_filter($names));
    if ($names) {
      ctools_export_to_hook_code($code, $table, $names, $module);
    }
  }

  $module_code = '';
  $apis = array();
  $files = array();
  $dependencies = array();
  foreach ($code as $module => $api_info) {
    if ($module == 'general') {
      $module_code .= $api_info;
    }
    else {
      foreach ($api_info as $api => $info) {
        $api_code = "  if (\$module == '$module' && \$api == '$api') {\n";
        $api_code .= "    return array('version' => $info[version]);\n";
        $api_code .= "  }\n";
        $apis[] = array('module' => $module, 'api' => $api, 'code' => $api_code);
        $dependencies[$module] = TRUE;

        $file = $module . '.' . $api . '.inc';
        $code = "<?php\n";
        $code .= "// \$Id" . ": $\n\n";
        $code .= "/**\n";
        $code .= " * @file\n";
        $code .= " * Bulk export of $api objects generated by Bulk export module.\n";
        $code .= " */\n\n";
        $code .= $info['code'];
        // TODO: if the file exists, check if it contains any exportables
        // which we're not exporting now. If that's the case, we need to build
        // our own code (steal from ctools_export_to_hook_code) and merge it
        // with what we got by loading the original file and merging the
        // object in
        $files[$file] = $code;
      }
    }
  }

  // TODO: check the module file to see if the api is defined, add it in if not.
  // Add hook_ctools_plugin_api at the top of the module code, if there is any.
  // if ($api_code) {
  //   $api =  "/**\n";
  //   $api .= " * Implementation of hook_ctools_plugin_api().\n";
  //   $api .= " */\n";
  //   $api .= "function $form_state[module]_ctools_plugin_api(\$module, \$api) {\n";
  //   $api .= $api_code;
  //   $api .= "}\n";
  //   $module_code = $api . $module_code;
  // }


  // if ($module_code) {
  //   $module =  "<?php\n";
  //   $module .= "// \$Id" . ": $\n\n";
  //   $module .= "/**\n";
  //   $module .= " * @file\n";
  //   $module .= " * Bulk export of objects generated by Bulk export module.\n";
  //   $module .= " */\n\n";
  //   $module .= $module_code;
  //   $output = drupal_get_form('ctools_export_form', $module, t('Place this in @file', array('@file' => $form_state['module'] . '.module'))) . $output;
  // }

  // TODO: Check the dependencies. Add in the missing.
  // $info  = "; \$Id" . ": $\n"; // The break in the string prevents CVS from subbing the ID.
  // $info .= strtr("name = @module export module\n", array('@module' => $form_state['module']));
  // $info .= strtr("description = Export objects from CTools\n", array('@module' => $form_state['values']['name']));
  // foreach ($dependencies as $module => $junk) {
  //   $info .= "dependencies[] = $module\n";
  // }
  // $info .= "package = Chaos tool suite\n";
  // $info .= "core = 6.x\n";


  
  
  drush_print(print_r(array(
                        'module_code' => $module_code,
                        'apis' => $apis,
                        'files' => $files,
                        'dependencies' => $dependencies), TRUE));
}
