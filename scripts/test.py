#!/usr/bin/env python
import subprocess
import argparse
import glob
import time
import sys
import os

import tests.test_net
import tests.test_tracing

from operator import attrgetter
from tests.testing import *

os.environ["LC_ALL"]="C"
os.environ["LANG"]="C"

blacklist= [
    "tst-dns-resolver.so",
]

add_tests([
    SingleCommandTest('java', '/java.so -cp /tests/java/tests.jar:/tests/java/isolates.jar \
        -Disolates.jar=/tests/java/isolates.jar org.junit.runner.JUnitCore io.osv.AllTests'),
    SingleCommandTest('java-perms', '/java.so -cp /tests/java/tests.jar io.osv.TestDomainPermissions'),
])

class TestRunnerTest(SingleCommandTest):
    def __init__(self, name):
        super(TestRunnerTest, self).__init__(name, '/tests/%s' % name)

test_files = set(glob.glob('build/release/tests/tst-*.so')) - set(glob.glob('build/release/tests/*-stripped.so'))
add_tests((TestRunnerTest(os.path.basename(x)) for x in test_files))

p = subprocess.Popen(["hostname", "--ip-address"],
                     stdin=subprocess.PIPE,
                     stdout=subprocess.PIPE,
                     stderr=subprocess.PIPE)
ip, err = p.communicate()
rc = p.returncode

nfs_tests = [ SingleCommandTest('nfs-test', "/tests-nfs/tst-nfs.so %s" %
                                ip.strip()) ]

def run_test(test):
    sys.stdout.write("  TEST %-35s" % test.name)
    sys.stdout.flush()

    start = time.time()
    try:
        test.run()
    except:
        sys.stdout.write("Test %s FAILED\n" % test.name)
        sys.stdout.flush()
        raise
    end = time.time()

    duration = end - start
    sys.stdout.write(" OK  (%.3f s)\n" % duration)
    sys.stdout.flush()

def is_not_skipped(test):
    return test.name not in blacklist

def run_tests_in_single_instance():
    run(filter(lambda test: not isinstance(test, TestRunnerTest), tests))

    blacklist_tests = ' '.join(blacklist)
    args = ["-s", "-e", "/testrunner.so -b %s" % (blacklist_tests)]
    if subprocess.call(["./scripts/run.py"] + args):
        exit(1)

def run(tests):
    for test in sorted(tests, key=attrgetter('name')):
        if is_not_skipped(test):
            run_test(test)
        else:
            sys.stdout.write("  TEST %-35s SKIPPED\n" % test.name)
            sys.stdout.flush()

def pluralize(word, count):
    if count == 1:
        return word
    return word + 's'

def run_tests():
    start = time.time()

    if cmdargs.nfs:
        tests_to_run = nfs_tests
    elif cmdargs.name:
        tests_to_run = list((t for t in tests if re.match('^' + cmdargs.name + '$', t.name)))
        if not tests_to_run:
            print('No test matches: ' + cmdargs.name)
            exit(1)
    else:
        tests_to_run = tests

    if cmdargs.nfs:
        # We need sudo because nfs port are below 1024.
        proc = subprocess.Popen(["sudo",
                                 "./build/release.x64/ganesha.nfsd",
                                 "-F",
                                 "-p",
                                 "../../nfs-tests/ganesha.pid",
                                 "-f",
                                 "../../nfs-tests/ganesha.conf"],
                                stdin = sys.stdin,
                                stdout = sys.stdout,
                                stderr = sys.stderr)
        run(tests_to_run)
        subprocess.call(["sudo", "kill", "-9", str(proc.pid)])
        proc.communicate()
    elif cmdargs.single:
        if tests_to_run != tests:
            print('Cannot restrict the set of tests when --single option is used')
            exit(1)
        run_tests_in_single_instance()
    else:
        run(tests_to_run)

    end = time.time()

    duration = end - start
    print(("OK (%d %s run, %.3f s)" % (len(tests_to_run), pluralize("test", len(tests_to_run)), duration)))

def main():
    while True:
        run_tests()
        if not cmdargs.repeat:
            break

if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='test')
    parser.add_argument("-v", "--verbose", action="store_true", help="verbose test output")
    parser.add_argument("-r", "--repeat", action="store_true", help="repeat until test fails")
    parser.add_argument("-s", "--single", action="store_true", help="run as much tests as possible in a single OSv instance")
    parser.add_argument("-n", "--nfs",    action="store_true", help="run nfs test in a single OSv instance")
    parser.add_argument("--name", action="store", help="run all tests whose names match given regular expression")
    cmdargs = parser.parse_args()
    set_verbose_output(cmdargs.verbose)
    main()
