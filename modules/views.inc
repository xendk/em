<?php

// $Id$

/**
 * @file
 *
 * Support file to allow views to be exported with drush em.
 */

/**
 * Implementation of hook_em_exportables() on behalf of the views module.
 */
function views_em_exportables() {
  $export_types = array();
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
  return $export_types;
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

function em_view_status($export) {
  if ($export->type == t('Overridden')) {
    return EM_STATUS_IN_DATABASE | EM_STATUS_IN_CODE;
  } elseif ($export->type == t('Standard')) {
    return EM_STATUS_IN_CODE;
  }
  // t('Normal')
  return EM_STATUS_IN_DATABASE;
}

/**
 * Helper function to delete view.
 */
function em_view_delete($view) {
  $view->delete();
}
