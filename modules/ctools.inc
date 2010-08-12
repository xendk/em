<?php

// $Id$

/**
 * @file
 *
 * Support file to allow ctools exportables to be exported with drush em.
 */

/**
 * Implementation of hook_em_exportables() on behalf of CTools.
 */
function ctools_em_exportables() {
  $export_types = array();
  // Find out what ctools exportables that's available.
  ctools_include('export');
  $schemas = ctools_export_get_schemas(TRUE);

  foreach ($schemas as $etable => $schema) {
    if (isset($schema['export']['api'])) {
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
    }
    if (empty($export_types[$etable]['list callback']) ||
        !function_exists($export_types[$etable]['list callback'])) {
      $export_types[$etable]['list callback'] = 'ctools_export_default_list';
      $export_types[$etable]['list arguments'] = array($etable, $schema);
    }
    if (empty($export_types[$etable]['export callback']) or !function_exists($export_types[$etable]['export callback'])) {
      $export_types[$etable]['export callback'] = 'ctools_export_object';
      $export_types[$etable]['export arguments'] = array($etable);
    }
  }
  return $export_types;
}

/**
 * Implementation of hook_em_exportables_alter() on behalf of CTools.
 */
function ctools_em_exportables_alter($export_types) {
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
    'panels_layout' => array(
      'delete callback' => 'ctools_export_crud_delete',
      'delete arguments' => array('panels_layout'),
    ),
  );
  foreach ($overrides as $name => $override) {
    if (isset($export_types[$name])) {
      $export_types[$name] = $override + $export_types[$name];
    }
  }
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

