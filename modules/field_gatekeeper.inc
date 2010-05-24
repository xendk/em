<?php

// $Id$

/**
 * @file
 *
 * Support file to allow field_gatekeeper fields to be exported with drush em.
 *
 * @todo Move this into the field_gatekeeper module.
 */

/**
 * Implementation of hook_em_exportables() on behalf of the field_gatekeeper module.
 */
function field_gatekeeper_em_exportables() {
  $export_types = array();
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

  if (module_exists('fieldgroup')) {
    $export_types['field_groups'] = array(
      'api' => 'field_groups',
      'identifier' => 'group',
      'key' => 'name',
      'list callback' => 'em_fg_get_all_groups',
      'default hook' => 'field_groups',
      'load callback' => 'em_fg_load_groups',
      //'export callback' => '',
      'to hook code callback' => 'em_fg_groups_to_hook_code',
      'status callback' => 'em_fg_group_status',
      // 'delete callback' => 'em_fg_instance_delete',
      'is array' => TRUE,
    );
  }
  return $export_types;
}

function em_fg_get_all_gatekeeper() {
  module_load_include('inc', 'content', '/includes/content.crud');
  $fields = array();
  // Module defined.
  $gw_fields = field_gatekeeper_get_field_gatekeeper();
  foreach ($gw_fields as $field) {
    $fields[$field['field_name']] = field_gatekeeper_base_clean($field);
  }
  // Load fields in DB from CCK.
  $all_fields = content_field_instance_read(NULL, TRUE);
  foreach ($all_fields as $field) {
    $fields[$field['field_name']] = field_gatekeeper_base_clean($field);
  }
  return $fields;
}

function em_fg_get_all_instances() {
  module_load_include('inc', 'content', '/includes/content.crud');
  $fields = array();
  // Module defined.
  $gw_fields = field_gatekeeper_get_field_instances();
  foreach ($gw_fields as $field) {
    $fields[$field['name']] = field_gatekeeper_instance_clean($field);
  }
  // Overlay fields in DB from CCK.
  $all_fields = content_field_instance_read(NULL, TRUE);
  foreach ($all_fields as $field) {
    $field = field_gatekeeper_instance_clean($field);
    $field['name'] = $field['type_name'] . '-' . $field['field_name'];
    $fields[$field['name']] = $field;
  }
  return $fields;
}

function em_fg_get_all_groups() {
  $groups = array();
  // TODO: Load from module?
  $all_groups = fieldgroup_groups();
  foreach ($all_groups as $type => $type_groups) {
    foreach ($type_groups as $group_name => $group) {
      $group['fields'] = array_keys($group['fields']);
      $group['name'] = $type . '-' . $group_name;
      $groups[$group['name']] = $group;
    }
  }
  return $groups;
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

function em_fg_load_groups($names) {
  $all_groups = em_fg_get_all_groups();
  $groups = array();
  foreach ($names as $name) {
    if ($all_groups[$name]) {
      $groups[$name] = $all_groups[$name];
    }
  }
  return $groups;
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
    $diffs = _field_gatekeeper_array_rdiff($field, $code_defined[$field['field_name']]);
    if ($diffs) {
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
    $diffs = _field_gatekeeper_array_rdiff($field, $code_defined[$field['name']]);
    if ($diffs) {
       return EM_STATUS_IN_DATABASE | EM_STATUS_IN_CODE;
    }
  }
  return EM_STATUS_IN_CODE;
}

function em_fg_group_status($group) {
  $type_groups = fieldgroup_groups($group['type_name']);
  if (!$type_groups[$group['group_name']]) {
    return EM_STATUS_IN_CODE;
  }
  
  $code_defined = field_gatekeeper_get_field_groups();
  if (!$code_defined[$group['name']]) {
    return EM_STATUS_IN_DATABASE;
  }
  $db_group = $type_groups[$group['group_name']];
  $db_group['name'] = $group['type_name'] . '-' . $group['group_name'];
  $db_group['fields'] = array_keys($db_group['fields']);

  $diffs = _field_gatekeeper_array_rdiff($group, $db_group);
  if ($diffs) {
    return EM_STATUS_IN_DATABASE | EM_STATUS_IN_CODE;
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

function em_fg_groups_to_hook_code($objects, $module) {
  $export = em_export('field_groups');
  $output = "/**\n";
  $output .= " * Implementation of hook_{$export['default hook']}().\n";
  $output .= " */\n";
  $output .= "function " . $module . "_{$export['default hook']}() {\n";

  foreach ($objects as $name) {
    list($type, $group_name) = explode('-', $name);
    if (!$group_name) {
      continue;
    }

    $type_groups = fieldgroup_groups($type);

    foreach ($all_groups as $type => $type_groups) {
    foreach ($type_groups as $group_name => $group) {
    }
  }
    
    if (!$type_groups[$group_name]) {
      continue;
    }
    $instance = $type_groups[$group_name];
    $instance['fields'] = array_keys($instance['fields']);
    $instance['name'] = $instance['type_name'] . '-' . $instance['group_name'];
    
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
