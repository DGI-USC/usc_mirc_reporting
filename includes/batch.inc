<?php

/**
 * @file
 * Miscellaneous batch-related functions.
 */

/**
 * Setup the batch to calculate file sizes and the number of title.
 *
 * @param callable $finished
 *   A callback for when the batch finishes. The results['report'] key should
 *   by used to the finished callback should be used to generate output.
 */
function usc_mirc_reporting_report_batch($finished) {
  $batch = array(
    'title' => 'USC MIRC Report Generation',
    'finished' => $finished,
    'operations' => array(
      array('usc_mirc_reporting_batch_file_sizes_and_type_count'),
      array('usc_mirc_reporting_batch_title_counts'),
    ),
  );
}

define('USC_MIRC_REPORTING_BATCH_SIZE', 100);

/**
 * Batch operation to report on file sizes.
 *
 * @param mixed $context
 *   An array (or ArrayObject) of batch context.
 */
function usc_mirc_reporting_batch_file_sizes_and_type_count(&$context) {
  // Types to iterate... We keep track of the "total" here, in addition to the
  // the models themselves.
  $type_to_iterate = array('total');

  // Get the content model URIs we use.
  $models = array();
  $models[] = $access = variable_get('usc_mirc_cmodel_access', 'usc:accessCModel');
  $models[] = $mezzanine = variable_get('usc_mirc_cmodel_mezzanine', 'usc:mezzanineCModel');
  $models[] = $vro = variable_get('usc_mirc_cmodel_vro', 'usc:vroCModel');

  // Add the models to the types to iterate over (when (un)serializing).
  $type_to_iterate += $models;

  if (!isset($context['sandbox']['offset'])) {
    // Some initialization.
    $context['sandbox']['offset'] = 0;
    foreach ($type_to_iterate as $type) {
      $context['results']['report']['types'][$type] = array(
        'size' => gmp_init(0, 10),
        'count' => 0,
      );
    }
  }
  else {
    // Increment our offset, to get the next set.
    $context['sandbox']['offset'] += USC_MIRC_REPORTING_BATCH_SIZE;

    // Unserialize our big numbers.
    foreach ($type_to_iterate as $type) {
      $context['results']['report']['types'][$type]['size'] = gmp_init($context['results']['report']['types'][$type]['size'], 10);
    }
  }

  // Query for all objects, ordered by created date and offset by the value
  // stored in the context.
  $limit = USC_MIRC_REPORTING_BATCH_SIZE;
  $query = <<<EOQ
  SELECT ?object ?date
  WHERE {
    {?object <fedora-model:hasModel> <info:fedora/$vro>}
    UNION
    {?object <fedora-model:hasModel> <info:fedora/$mezzanine>}
    UNION
    {?object <fedora-model:hasModel> <info:fedora/$access>}
    ?object <fedora-model:createdDate> ?date ;
            <fedora-view:disseminates> [<fedora-view:disseminationType> <info:fedora/*/PBCORE>]
  }
  ORDER BY ?date
  OFFSET {$context['sandbox']['offset']}
  LIMIT $limit
EOQ;

  $tuque = islandora_get_tuque_connection():
  $results = $tuque->repository->ri->sparqlQuery($query);

  $type_report = &$context['results']['report']['types'];
  foreach ($results as $result) {
    $object = islandora_object_load($result['object']['value']);

    $intersected_models = array_intersect($models, $object->models);
    if (!empty($intersected_models) {
      if (count($intersected_models) > 1) {
        watchdog('usc_mirc_reporting', 'Somehow have multiple models on @pid: @models. Skipping in report.', array(
          '@pid' => $object->id,
          '@models' => implode(', ', $intersected_models),
        ));
        continue;
      }

      // Should have exactly one model... Let's get it!
      $type = reset($intersected_models);

      // Increment the count.
      $type_report[$type]['count']++;

      // Get all the sizes (PBCore description doc could contain multiple).
      $sizes = usc_mirc_reporting_get_size_from_pbcore($object['PBCORE']);
      foreach ($sizes as $size) {
        $type_report[$type]['size'] = gmp_add($type_report[$type]['size'], $size);
      }
    }
    else {
      watchdog('usc_mirc_reporting', 'You found an anomaly!... @pid was somehow selected as having a content model, but then when we checked the object, we couldn\'t find any we were interested in...', array(
        '@pid' => $object->id,
      ));
    }
  }

  if (count($results) < USC_MIRC_REPORTING_BATCH_SIZE) {
    // If we didn't get a full set of results, we're finished.
    $context['finished'] = 1;

    // Accumulate our result into the "totals".
    foreach ($models as $model) {
      $type_report['total']['size'] = gmp_add($type_report['total']['size'], $type_report[$model]['size']);
      $type_report['total']['count'] += $type_report[$model]['count'];
    }
  }
  else {
    // Otherwise, keep on trucking (we'll increment the offset next time
    // around).
    $context['finished'] = 0;
  }

  // Serialize the large numbers, so we can can use them next time.
  foreach ($type_to_iterate as $type) {
    $type_report[$type]['size'] = gmp_strval($type_report[$type]['size'], 10);
  }
}

/**
 * Get sizes from any instantiations present in the given datastream.
 *
 * @param AbstractDatastream $pbcore_datastream
 *   A datastream we expect to contain PBCore XML.
 *
 * @return array
 *   An array of strings, each of which represents the size of an
 *  instantiation from the given PBCore datastream.
 */
function usc_mirc_reporting_get_sizes_from_pbcore(AbstractDatastream $pbcore_datastream) {
  $to_return = array();

  $pbcore_simplexml = new SimpleXMLElement($pbcore_datastream->content);
  if ($pbcore_simplexml) {
    $pbcore_simplexml->registerXPathNamespace('pb', 'http://www.pbcore.org/PBCore/PBCoreNamespace.html');
    $file_size_xpath = '//pb:instantiationFileSize[@unitsOfMeasure="bytes"]';
    foreach((array)$pbcore_simplexml->xpath($file_size_xpath) as $size) {
      $size = (string)$size;
      $to_return[] = trim($size);
    }
  }
  else {
    watchdog('usc_mirc_reporting', 'Failed to load PBCORE from @pid, when looking for file sizes.', array(
      '@pid' => $pbcore_datastream->parent->id,
    ));
  }

  return $to_return;
}

/**
 * Batch operation to report on the number of unique titles.
 *
 * @param mixed $context
 *   An array (or ArrayObject) of batch context.
 */
function usc_mirc_reporting_batch_title_counts(&$context) {

}
