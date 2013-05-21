<?php

/**
 * @file
 * Various theme implementations.
 */

/**
 * Implements template_preprocess_HOOK().
 */
function template_preprocess_usc_mirc_reporting_report_table(array &$vars) {
  $o = array(
    'header' => array(
      t('Sample Point'),
      t('All assets'),
      t('Access copies'),
      t('Mezzanines'),
      t('Preservation masters'),
      t('Accessible titles'),
      t('In progress titles'),
      t('Total titles'),
    ),
    'colgroups' => array(
      array(
        array(
          'class' => array('sample-point'),
        ),
        array(
          'class' => array('sizes-per-type'),
          'span' => 4,
        ),
        array(
          'class' => array('title-counts'),
          'span' => 3,
        ),
      ),
    ),
    'rows' => array(),
  );

  foreach ($vars['report_structure']['intervals'] as $interval) {
    $row = array(
      format_date($interval, 'short'),
    );

    $type_report = $vars['report_structure']['types'][$interval];

    foreach ($type_report as $type => $values) {
      $row[] = format_plural($values['count'], '@size in 1 asset', '@size in @count assets', array(
        '@size' => format_size($values['size']),
      ));
    }

    foreach ($vars['report_structure']['titles'][$interval] as $value) {
      $row[] = format_plural($value, '1 unique title', '@count unique titles');
    }

    $o['rows'][] = $row;
  }

  $vars['table'] = theme('table', $o);
}
