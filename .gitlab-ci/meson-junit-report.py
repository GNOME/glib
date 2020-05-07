#!/usr/bin/env python3

# Turns a Meson testlog.json file into a JUnit XML report
#
# Copyright 2019  GNOME Foundation
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Original author: Emmanuele Bassi

import argparse
import datetime
import json
import os
import sys
import xml.etree.ElementTree as ET

aparser = argparse.ArgumentParser(description='Turns a Meson test log into a JUnit report')
aparser.add_argument('--project-name', metavar='NAME',
                     help='The project name',
                     default='unknown')
aparser.add_argument('--job-id', metavar='ID',
                     help='The job ID for the report',
                     default='Unknown')
aparser.add_argument('--branch', metavar='NAME',
                     help='Branch of the project being tested',
                     default='master')
aparser.add_argument('--output', metavar='FILE',
                     help='The output file, stdout by default',
                     type=argparse.FileType('w', encoding='UTF-8'),
                     default=sys.stdout)
aparser.add_argument('infile', metavar='FILE',
                     help='The input testlog.json, stdin by default',
                     type=argparse.FileType('r', encoding='UTF-8'),
                     default=sys.stdin)

args = aparser.parse_args()

outfile = args.output

testsuites = ET.Element('testsuites')
testsuites.set('id', '{}/{}'.format(args.job_id, args.branch))
testsuites.set('package', args.project_name)
testsuites.set('timestamp', datetime.datetime.utcnow().isoformat())

suites = {}
for line in args.infile:
    data = json.loads(line)
    (full_suite, unit_name) = data['name'].split(' / ')
    try:
        (project_name, suite_name) = full_suite.split(':')
    except ValueError:
        project_name = full_suite
        suite_name = full_suite

    duration = data['duration']
    return_code = data['returncode']
    log = data['stdout']
    log_stderr = data.get('stderr', '')

    unit = {
        'suite': suite_name,
        'name': unit_name,
        'duration': duration,
        'returncode': return_code,
        'stdout': log,
        'stderr': log_stderr,
    }

    units = suites.setdefault(suite_name, [])
    units.append(unit)

for name, units in suites.items():
    print('Processing suite {} (units: {})'.format(name, len(units)))

    def if_failed(unit):
        if unit['returncode'] != 0:
            return True
        return False

    def if_succeded(unit):
        if unit['returncode'] == 0:
            return True
        return False

    successes = list(filter(if_succeded, units))
    failures = list(filter(if_failed, units))
    print(' - {}: {} pass, {} fail'.format(name, len(successes), len(failures)))

    testsuite = ET.SubElement(testsuites, 'testsuite')
    testsuite.set('name', '{}/{}'.format(args.project_name, name))
    testsuite.set('tests', str(len(units)))
    testsuite.set('errors', str(len(failures)))
    testsuite.set('failures', str(len(failures)))

    for unit in successes:
        testcase = ET.SubElement(testsuite, 'testcase')
        testcase.set('classname', '{}/{}'.format(args.project_name, unit['suite']))
        testcase.set('name', unit['name'])
        testcase.set('time', str(unit['duration']))

    for unit in failures:
        testcase = ET.SubElement(testsuite, 'testcase')
        testcase.set('classname', '{}/{}'.format(args.project_name, unit['suite']))
        testcase.set('name', unit['name'])
        testcase.set('time', str(unit['duration']))

        failure = ET.SubElement(testcase, 'failure')
        failure.set('classname', '{}/{}'.format(args.project_name, unit['suite']))
        failure.set('name', unit['name'])
        failure.set('type', 'error')
        failure.text = unit['stdout'] + '\n' + unit['stderr']

output = ET.tostring(testsuites, encoding='unicode')
outfile.write(output)
