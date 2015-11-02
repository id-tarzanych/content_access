<?php

/**
 * @file Content access administration UI.
 */

use Drupal\Core\Database\Database;
use Drupal\Core\Form\FormStateInterface;
use Drupal\Core\Template\Attribute;
use Drupal\user\Entity\Role;
use Drupal\Core\Cache\Cache;
use Drupal\node\Entity\Node;

/**
 * Specifies the threshold until we try to mass update node grants immediately.
 */
define('CONTENT_ACCESS_MASS_UPDATE_THRESHOLD', 1000);

/**
 * Submit callback for reset on content_access_page().
 */
function content_access_page_reset($form, FormStateInterface $form_state) {
  $storage = $form_state->getStorage();
  content_access_delete_per_node_settings($storage['node']);
  \Drupal::entityManager()->getAccessControlHandler('node')->writeGrants($storage['node']);

  drupal_set_message(t('The permissions have been reseted to the content type defaults.'));
}


/**
 * Mass updates node access records for nodes of the given types.
 * @param $types
 *   An array of content type names.
 * @return
 *   Whether the operation has been processed successfully or postponed.
 */
function content_access_mass_update($types) {
  $query = Database::getConnection()->select('node', 'n')
    ->fields('n', array('nid'))
    ->condition('n.type', $types, 'IN');

  $count = $query->countQuery()->execute()->fetchField();

  node_access_needs_rebuild(TRUE);

  // If there not too much nodes affected, try to do it.
  if ($count <= CONTENT_ACCESS_MASS_UPDATE_THRESHOLD) {
    $records = $query->execute();

    foreach ($records as $node) {
      $node = Node::load($node->nid);
      \Drupal::entityManager()->getAccessControlHandler('node')->writeGrants($node);
    }

    foreach (Cache::getBins() as $service_id => $cache_backend) {
      $cache_backend->deleteAll();
    }

    node_access_needs_rebuild(FALSE);

    return TRUE;
  }

  return FALSE;
}

/**
 * Saves the given permissions by role to the database.
 */
function content_access_save_permissions($roles_permissions) {
  foreach ($roles_permissions as $rid => $permissions) {
    user_role_change_permissions($rid, $permissions);
  }
}

/**
 * Builds the role based permission form for the given defaults.
 *
 * @param $defaults
 *   Array of defaults for all operations.
 */
function content_access_role_based_form(&$form, $defaults = array(), $type = NULL) {
  $description = [
    t('Note that users need at least the %access_content permission to be able to deal in any way with content.', [
      '%access_content' => t('access content'),
    ]),
    t('Furthermore note that content which is not @published is treated in a different way by drupal: It can be viewed only by its author or users with the %administer_nodes permission.', [
      '@published' => t('published'),
      '%administer_nodes' => t('administer nodes'),
    ]),
  ];
  $form['per_role'] = array(
    '#type' => 'fieldset',
    '#title' => t('Role based access control settings'),
    '#collapsible' => TRUE,
    '#description' => implode(' ', $description),
  );

  $operations = _content_access_get_operations($type);
  $user_roles = Role::loadMultiple();
  $roles = array();
  foreach ($user_roles as $role) {
    $roles[$role->id()] = $role->get('label');
  }
  foreach ($operations as $op => $label) {
    // Make sure defaults are set properly
    $defaults += array($op => array());

    $form['per_role'][$op] = array(
      '#type' => 'checkboxes',
      '#prefix' => '<div class="content_access-div">',
      '#suffix' => '</div>',
      '#options' => $roles,
      '#title' => $label,
      '#default_value' => $defaults[$op],
    );
  }

  $form['per_role']['clearer'] = array(
    '#value' => '<br clear="all" />',
  );

  $form['#attached']['library'][] = 'content_access/drupal.content_access';

  return $form;
}

/**
 * Formapi #after_build callback, that disables checkboxes for roles without access to content.
 */
function content_access_force_permissions($element, FormStateInterface $form_state) {
  $storage = $form_state->getStorage();
  if (!empty($storage['node'])) {
    $node = $storage['node'];
    foreach (['update', 'update_own', 'delete', 'delete_own'] as $op) {
      foreach (content_access_get_settings($op, $node->getType()) as $rid) {
        $element[$op][$rid]['#disabled'] = TRUE;
        $element[$op][$rid]['#attributes']['disabled'] = 'disabled';
        $element[$op][$rid]['#value'] = TRUE;
        $element[$op][$rid]['#checked'] = TRUE;

        $prefix_attr = new Attribute([
          'title' => t('Permission is granted due to the content type\'s access control settings.'),
        ]);
        $element[$op][$rid]['#prefix'] = '<span ' . $prefix_attr . '>';
        $element[$op][$rid]['#suffix'] = "</span>";
      }
    }
  }

  return $element;
}

/**
 * Submit callback for the user permissions form.
 * Trigger changes to node permissions to rebuild our grants.
 */
function content_access_user_admin_perm_submit($form, FormStateInterface $form_state) {
  // Check for each content type, which has per node access activated
  // whether permissions have been changed.
  $types = array();
  foreach (array_filter(content_access_get_settings('per_node', 'all')) as $type => $value) {
    foreach (_content_access_get_node_permissions($type) as $perm) {
      foreach (user_roles() as $rid => $role) {
        $values = $form_state->getValues();
        if (isset($values[$rid]) && in_array($perm, $form['checkboxes'][$rid]['#default_value']) != in_array($perm, $values[$rid])) {
          // permission changed.
          $types[$type] = node_type_get_names();
          continue 2;

        }
      }
    }
  }
  if ($types && content_access_mass_update(array_keys($types))) {
    drupal_set_message(\Drupal::translation()->formatPlural(count($types),
      'Permissions have been successfully rebuilt for the content type @types.',
      'Permissions have been successfully rebuilt for the content types @types.',
      ['@types' => implode(', ', $types)]
    ));
  }
}
