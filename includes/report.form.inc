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
    batch_set(usc_mirc_reporting_report_batch());
  }
  elseif ($triggering_element_id == 'edit-download-csv') {
    // TODO: Cause downoad of a CSV copy of the results.
  }
  elseif ($triggering_element_id == 'edit-delete-report') {
    variable_del('usc_mirc_reporting_report');
  }
}
