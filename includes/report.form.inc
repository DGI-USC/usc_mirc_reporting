<?php

/**
 * @file
 * Form definition for reporting.
 */

/**
 * Form building function.
 */
function usc_mirc_reporting_form($form, &$form_state) {
  form_load_include($form_state, 'inc', 'usc_mirc_reporting', 'includes/report.form');

  $report = variable_get('usc_mirc_reporting_report', FALSE);
  $mutate_report = usc_mirc_reporting_mutate_report_access_callback();
  if ($report) {
    $form['report'] = array(
      '#type' => 'item',
      '#title' => t('Report generated @date', array(
        '@date' => format_date($report['date']),
      )),
      '#markup' => theme('usc_mirc_reporting_report_table', array(
        'report_structure' => $report,
      )),
    );
    $form['download_csv'] = array(
      '#type' => 'submit',
      '#value' => t('Download as CSV'),
    );
    $form['delete_report'] = array(
      '#type' => 'submit',
      // Only allow those with permission to generate the report to delete it.
      '#access' => $mutate_report,
      '#value' => t('Delete the report'),
    );
  }
  else {
    $form['report'] = array(
      '#type' => 'item',
      '#title' => t('No report!'),
      '#markup' => t('The report has not yet been generated.'),
    );
  }

  $form['regenerate'] = array(
    '#type' => 'submit',
    '#access' => $mutate_report,
    '#value' => t('(Re)generate report'),
  );

  return $form;
}

/**
 * Form submit callback.
 */
function usc_mirc_reporting_form_submit(&$form, &$form_state) {
  $triggering_element_id = $form_state['triggering_element']['#id'];

  if ($triggering_element_id == 'edit-regenerate') {
    module_load_include('inc', 'usc_mirc_reporting', 'includes/batch');

    $now = time();
    $intervals = array(
      $now,
    );
    for ($i = 0; $i < 12; $i++) {
      $intervals[] = strtotime("midnight first day of $i months ago", $now);
    }

    batch_set(usc_mirc_reporting_report_batch($intervals));
  }
  elseif ($triggering_element_id == 'edit-download-csv') {
    // Cause downoad of a CSV copy of the results.
    $form_state['redirect'] = 'admin/reports/usc_mirc/csv';
  }
  elseif ($triggering_element_id == 'edit-delete-report') {
    variable_del('usc_mirc_reporting_report');
  }
}

/**
 * An access check if the current user has admin permissions.
 *
 * Admin permissions--the ability to always pass XACML restrictions--is
 * required to generate the report.
 */
function usc_mirc_reporting_mutate_report_access_callback() {
  global $user;
  $administrator_roles = array('administrator');

  return $user->uid === 1 || (count(array_intersect($administrator_roles, $user->roles)) > 0);
}
