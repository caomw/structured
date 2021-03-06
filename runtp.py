#!/usr/bin/env python

# structured - Tools for the Generation and Visualization of Large-scale
# Three-dimensional Reconstructions from Image Data. This software includes
# source code from other projects, which is subject to different licensing,
# see COPYING for details. If this project is used for research see COPYING
# for making the appropriate citations.
# Copyright (C) 2013 Matthew Johnson-Roberson <mattkjr@gmail.com>
#
# This file is part of structured.
#
# structured is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# structured is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with structured.  If not, see <http://www.gnu.org/licenses/>.

# -*- coding: utf-8 -*-

# structured - Tools for the Generation and Visualization of Large-scale
# Three-dimensional Reconstructions from Image Data. This software includes
# source code from other projects, which is subject to different licensing,
# see COPYING for details. If this project is used for research see COPYING
# for making the appropriate citations.
# Copyright (C) 2013 Matthew Johnson-Roberson <mattkjr@gmail.com>
#
# This file is part of structured.
#
# structured is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# structured is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with structured.  If not, see <http://www.gnu.org/licenses/>.

from threadpool import *
from progressbar import *
from time import sleep
from random import random
import sys
import os
import thread
import commands
import random
def detectCPUs():
    """
    Detects the number of CPUs on a system. Cribbed from pp.
    """
    # Linux, Unix and MacOS:
    if hasattr(os, "sysconf"):
        if os.sysconf_names.has_key("SC_NPROCESSORS_ONLN"):
            # Linux & Unix:
            ncpus = os.sysconf("SC_NPROCESSORS_ONLN")
        if isinstance(ncpus, int) and ncpus > 0:
            return ncpus
        else: # OSX:
            return int(os.popen2("sysctl -n hw.ncpu")[1].read())
    # Windows:
    if os.environ.has_key("NUMBER_OF_PROCESSORS"):
       ncpus = int(os.environ["NUMBER_OF_PROCESSORS"]);
    if ncpus > 0:
        return ncpus
    return 1 # Default


if len(sys.argv) > 2:
    numthreads=int(sys.argv[2])
else:
    numthreads=detectCPUs()

print 'ThreadPool spawning %d threads' % numthreads
main = ThreadPool(numthreads)



def runcmd_threadpool(cm):
    cmd= 'csh -c \'%s\'' % cm[:-1]
    s,o = commands.getstatusoutput(cmd)
    if s != 0:
        print 'problem running: ', cmd
        print 'output : ',o
        errfn='./%s/%d-cmd-%03d.txt'% ('.',os.getpid(),random.random())
        print 'wrote log to %s ' % errfn
        f = open(errfn, 'w')
        f.write( 'problem running: %s' % cmd)
        f.write('output : %s' % o)
        f.close()
    return 0
   
# this will be called each time a result is available
def print_result(request, result):
    pbar.inc()
# this will be called when an exception occurs within a thread
# this example exception handler does little more than the default handler
def handle_exception(request, exc_info):
        if not isinstance(exc_info, tuple):
            # Something is seriously wrong...
            print request
            print exc_info
            raise SystemExit
        print "**** Exception occured in request #%s: %s" % \
          (request.requestID, exc_info)
    
class Curr(ProgressBarWidget):
    "Just the percentage done."
    def update(self, pbar):
        return '%03d' % pbar.currval
    
class Max(ProgressBarWidget):
    "Just the percentage done."
    def update(self, pbar):
        return '%03d' % pbar.maxval


fname = sys.argv[1];
cmd_counter = 0


if len(sys.argv) > 3:
    title = sys.argv[3]
else:
    title = 'Cmds: '


widgets = [title, ' ',Percentage(), ' ',Curr(),'/',Max(),' ', Bar(),
                   ' ', ETA()]

dir,end = os.path.split(fname)
try:
    cmdfile = open(fname, 'r')
except IOError:
    print 'Cannot open file %s for reading. Check file exists and try again.' % fname
    sys.exit(0)
total=0
i=0
total=sum(1 for line in cmdfile)
cmdfile.close()
cmdfile = open(fname, 'r')
pbar = ProgressBar(widgets=widgets, maxval=total)
pbar.start()
data = []
for line in cmdfile:
    data.append(line)
#    runcmd_threadpool(line,i,total,dir)
    i=i+1;
requests = makeRequests(runcmd_threadpool, data, print_result, handle_exception)
for req in requests:
    main.putRequest(req)
   # print "Work request #%s added." % req.requestID
pbar.start()
main.wait()

pbar.finish()

print ''

f = open('timing.txt', 'a')
f.write('%s %f\n' % (title,pbar.seconds_elapsed))
f.close()
#@threaded
#def test_threaded(i):
#    print 'threaded %i enter' % i
 #   sleep(random())
 #   print 'threaded %i exit' % i

#print 'threaded example'
#for i in range(5):
#    test_threaded(i)
#print 'done'