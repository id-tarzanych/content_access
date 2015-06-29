<?php

/**
 * @file
 * Contains \Drupal\content_access\Controller\ContentAccessController.
 */

namespace Drupal\content_access\Controller;

use Drupal\Core\Controller\ControllerBase;
use Drupal\node\Entity\Node;

/**
 * Controller routines for user routes.
 */
class ContentAccessController extends ControllerBase {

  /**
   * Returns content access settings page title.
   */
  public function getContentAccessTitle() {
    $nid = \Drupal::routeMatch()->getParameter('node');
    $node = Node::load($nid);
    $title = t('Access control for <em>@title</em>', array('@title' => $node->getTitle()));

    return $title;
  }

}
