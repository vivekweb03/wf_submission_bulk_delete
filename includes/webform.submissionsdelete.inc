<?php

/**
 * @file
 * This file includes helper functions for  deleting webform submissions for wf_submission_bulk_delete.module.
 */

// All functions within this file need the webform.submissions.inc.
module_load_include('inc', 'webform', 'includes/webform.submissions');

/**
 * Form for deleting webform submissions based on IP Address.
 */
function wf_submissions_delete_by_ip_form($form, &$form_state, $node) {
  $form = array();

  $form['nid'] = array(
    '#type' => 'value',
    '#value' => $node->nid
  );
  $form['wf_ip_address'] = array(
    '#type' => 'textfield',
    '#title' => t('IP Address'),
    '#size' => 20,
    '#attributes' => array(
      'placeholder' => 'Enter IP address',
    ),
    '#description' => t('Delete webform submissions by IP address.'),
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Delete'),
  );

  return $form;
}

/**
 * Form submit handler function.
 */
function wf_submissions_delete_by_ip_form_submit($form, &$form_state) {
  $wf_ip_address = $form_state['values']['wf_ip_address'];
  $nid = $form_state['values']['nid'];

  $node = node_load($nid);
  // Set a modest batch size, due to the inefficiency of the hooks invoked when
  // submissions are deleted.
  $batch_size = 500;

  // Set up a batch to clear the results.
  $batch = array(
    'operations' => array(
      array('wf_submission_bulk_delete_batch_rows', array($node, $batch_size, $wf_ip_address)),
    ),
    'finished' => 'wf_submission_delete_batch_finished',
    'title' => t('Clear submissions'),
    'init_message' => t('Clearing submission data'),
    'error_message' => t('The submissions could not be cleared because an error occurred.'),
    'file' => drupal_get_path('module', 'wf_submission_bulk_delete') . '/includes/webform.submissionsdelete.inc',
  );
  batch_set($batch);
  $form_state['redirect'] = 'node/' . $nid . '/webform-results';

}

/**
 * Batch API callback.
 */
function wf_submission_bulk_delete_batch_rows($node, $batch_size, $wf_ip_address, &$context) {
  // Initialize the results if this is the first execution of the batch
  // operation.
  if (!isset($context['results']['count'])) {
    $context['results'] = array(
      'count' => 0,
      'total' => count(webform_get_submissions_by_ip($node->nid, $batch_size, $wf_ip_address)),
      'node' => $node,
    );
  }

  // Clear a batch of submissions.
  $count = wf_submission_bulk_delete_process($node->nid, $batch_size, $wf_ip_address);
  $context['results']['count'] += $count;

  // Display status message.
  $context['message'] = t('Cleared @count of @total submissions...', array('@count' => $context['results']['count'], '@total' => $context['results']['total']));
  $context['finished'] = $count > 0
                            ? $context['results']['count'] / $context['results']['total']
                            : 1.0;
}

/**
 * Delete all submissions for a node based on IP address.
 *
 * @param $nid
 *   The node id whose submissions will be deleted.
 * @param $batch_size
 *   The number of submissions to be processed. NULL means all submissions.
 * @param $wf_ip_address
 *   The IP address.
 *
 * @return int
 *   The number of submissions processed.
 */
function wf_submission_bulk_delete_process($nid, $batch_size = NULL, $wf_ip_address) {
  $node = node_load($nid);
  $submissions = webform_get_submissions_by_ip($nid, $batch_size, $wf_ip_address);
  $count = 0;
  foreach ($submissions as $submission) {
    webform_submission_delete($node, $submission);
    $count++;
  }
  return $count;
}

/**
 * Getting webform submissions of a node by IP address.
 */
function webform_get_submissions_by_ip($nid, $batch_size, $wf_ip_address) {
  // Get webform submissions from DB.
  $query = db_select('webform_submissions', 'ws');
  $query->fields('ws', array('sid'));
  $query->distinct();
  $query->join('webform_submitted_data', 'wsd', 'ws.nid=wsd.nid AND ws.sid=wsd.sid');
  $query->condition('ws.nid', $nid, '=');
  $query->condition('ws.remote_addr', $wf_ip_address, '=');
  $result = $query->execute()->fetchCol();

  // Call to webform module function.
  // To get webform submissions of a node.
  $submissions = webform_get_submissions($nid, NULL, $batch_size);
  // Filter submissions by IP address.
  $submissions_new = array();
  foreach ($result as $sid) {
    if (isset($submissions[$sid]) && !empty($submissions[$sid])) {
      $submissions_new[$sid] = $submissions[$sid];
    }
  }

  return $submissions_new;
}

/**
 * Batch API completion callback; Finish clearing submissions.
 */
function wf_submission_delete_batch_finished($success, $results, $operations) {
  if ($success) {
    $title = $results['node']->title;
    drupal_set_message(t('Webform %title entries cleared.', array('%title' => $title)));
    watchdog('webform', 'Webform %title entries cleared.', array('%title' => $title));
  }
}
