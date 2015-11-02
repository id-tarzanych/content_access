<?php


/**
 * @file
 * Contains Drupal\content_access\Access\ContentAccessNodePageAccessCheck.
 */

namespace Drupal\content_access\Access;

use Drupal\Core\Routing\Access\AccessInterface;
use Drupal\Core\Session\AccountInterface;
use Drupal\Core\Access\AccessResult;
use Drupal\node\Entity\Node;
use Drupal\Core\Routing\RouteMatchInterface;
use Symfony\Component\Routing\Route;


/**
 * Determines access to routes based on permissions defined via
 * $module.permissions.yml files.
 */
class ContentAccessNodePageAccessCheck implements AccessInterface {

  /**
   * {@inhericdoc}
   */
  public function access(AccountInterface $account, RouteMatchInterface $route_match) {
    $node = $route_match->getParameter('node');
    $all_nodes_access = $account->hasPermission('grant content access');
    $own_node_access = $account->hasPermission('grant own content access') && ($account->id() == $node->getOwnerId());

    return AccessResult::allowedIf(content_access_get_settings('per_node', $node->getType()) && ($all_nodes_access || $own_node_access));
  }

}
