<?php

// $Id$

/**
 * @file
 *
 * Support file to allow strongarm variables to be exported with drush em.
 */

/**
 * Implementation of hook_em_exportables_alter() on behalf of CTools.
 */
function strongarm_em_exportables_alter($export_types) {
  static $overrides = array(
    'variable' => array(
      // Stongarm 2.0 RC1 defines a non-existent export callback.
      'export callback' => 'em_export_strongarm',
      'status callback' => 'em_strongarm_status',
      'delete callback' => 'em_strongarm_delete',
      'list callback' => 'em_strongarm_list',
    ),
  );
  foreach ($overrides as $name => $override) {
    if ($export_types[$name]) {
      $export_types[$name] = $override + $export_types[$name];
    }
  }
}

function em_export_strongarm($variable, $indent = '') {
  return ctools_export_object('variable', $variable, $indent);
}

function em_strongarm_status($export) {
  global $conf;
  if ($export->export_type & EXPORT_IN_CODE) {
    $default = ctools_get_default_object('variable', $export->name);
    if (!$default) {
      return EM_STATUS_IN_DATABASE;
    }

    // If variable value does not match global $conf, this value has been
    // hardcoded (e.g. in settings.php) and has been allowed to pass
    // through. It cannot be reverted.
    if ($conf[$export->name] !== $export->value) {
      return EM_STATUS_IN_CODE;
    }
    else {
      return ($export->value == $default->value) ? EM_STATUS_IN_CODE : (EM_STATUS_IN_DATABASE | EM_STATUS_IN_CODE);
    }
  }
  return EM_STATUS_IN_CODE;
}

function em_strongarm_delete($export) {
  variable_del($export->name);
  return;
}

/**
 * Returns a listing of all variables.
 */
function em_strongarm_list() {
  $result = array();
  $res = db_query("SELECT name FROM {variable}");
  while ($row = db_fetch_object($res)) {
    $result[$row->name] = TRUE;
  }
  return $result;
}
