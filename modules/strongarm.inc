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
  // Due to the nature of strongarm, exports are never overridden.
  return EM_STATUS_IN_CODE;
}

function em_strongarm_delete($export) {
  // As strongarm can't be overridden, reverting is a noop.
  return;
}
