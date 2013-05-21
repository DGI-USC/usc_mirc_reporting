<?php

/**
 * @file
 * Callbacks used in CSV generation.
 */

/**
 * Build the CSV row structure.
 */
function usc_mirc_reporting_build_csv_structure() {
  $report = variable_get('usc_mirc_reporting_report', FALSE);

  $o = array(
    'header' => array(
      t('Sample Point'),
      t('All assets (count)'),
      t('All assets (bytes)'),
      t('Access copies (count)'),
      t('Access copies (bytes)'),
      t('Mezzanines (count)'),
      t('Mezzanines (bytes)'),
      t('Preservation masters (count)'),
      t('Preservation masters (bytes)'),
      t('Accessible titles'),
      t('In progress titles'),
      t('Total titles'),
    ),
    'rows' => array(),
    'date' => $report['date'],
  );


  foreach ($report['intervals'] as $interval) {
    $row = array(
      gmdate('c', $interval),
    );

    $type_report = $report['types'][$interval];

    foreach ($type_report as $type => $values) {
      $row[] = $values['count'];
      $row[] = $values['size'];
    }

    foreach ($report['titles'][$interval] as $value) {
      $row[] = $value;
    }

    $o['rows'][] = $row;
  }

  return $o;
}

/**
 * Delivery callback used to provide the CSV download.
 */
function usc_mirc_reporting_deliver_csv($page_result) {
  module_load_include('inc', 'phpexcel');

  $csv_file = fopen('php://temp', 'rb+');
  fputcsv($csv_file, $page_result['header']);
  foreach ($page_result['rows'] as $row) {
    fputcsv($csv_file, $row);
  }

  $length = ftell($csv_file);
  fseek($csv_file, 0);

  drupal_add_http_header('Cache-Control', 'no-cache');
  drupal_add_http_header('Content-Type', 'text/csv; charset=utf8');
  drupal_add_http_header('Content-Disposition', 'attachment;filename=report.'. $page_result['date'] .'.csv');
  drupal_add_http_header('Content-Length', $length);

  fpassthru($csv_file);
  fclose($csv_file);
}
