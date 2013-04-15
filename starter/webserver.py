#!/usr/bin/env python

from flask import Flask, redirect, url_for, request, render_template
import sys
import os
import hashlib
from socket import *
import urllib
from werkzeug import secure_filename
import tempfile
import shutil

ALLDIRS = ['/afs/andrew.cmu.edu/usr4/worichte/flask_env/lib/python2.6/site-packages']

import sys 
import site 

# Remember original sys.path.
prev_sys_path = list(sys.path) 

# Add each new site-packages directory.
for directory in ALLDIRS:
   	site.addsitedir(directory)

	# Reorder sys.path so new directories at the front.
	new_sys_path = [] 
	for item in list(sys.path): 
		if item not in prev_sys_path: 
			new_sys_path.append(item) 
			sys.path.remove(item) 
			sys.path[:0] = new_sys_path 

# the format that allowed to be stored on the distributed server
ALLOWED_EXTENSIONS = set(['txt', 'bmp', 'jpg', 'css', 'gif', 'png', 'jpeg'])

app = Flask(__name__)

"""
give the web browser the 'Content-Type'
"""
def change_type(url, response):
    type = {'.text':'text/plain', '.bmp':'/image/bmp', '.jpg':'image/jpeg', '.css':'text/css', '.gif':'image/gif', '.png':'image/png', '.jpeg':'image/jpeg'}

    for key in type:
        if url.find(key) != -1:
            response.headers['Content-type'] = type[key]
            return

    response.headers['Content-type'] = 'text/html'
    return

"""
show the default index.html page
"""
@app.route('/')
def index():
    return redirect(url_for('static', filename='index.html'))

"""
GET request from client
"""
@app.route('/rd/<int:local_port>', methods=["GET"])
def rd_getfile(local_port):
    # get the object name from request
    app.logger.debug("in rd_getfile obj is %s\n", request.args.get('object'))
    obj = request.args.get('object')
    if(None == obj):
        return render_template('501.html')
    
    # request to rd
    rd_req = 'GETRD ' + str(len(obj)) + ' ' + obj
    print 'request to rd: %s' % rd_req
    try:
        s = socket(AF_INET, SOCK_STREAM)
    except Exception:
        print 'Fail to create socket'
        return render_template('500.html')
    
    try:                                                
        s.connect(("127.0.0.1", local_port))
    except Exception:
        print 'Fail to connect rd'
        return render_template('500.html')

    if (s.send(rd_req) != len(rd_req)):
        print 'error: fail to send request to rd server.'
        s.close()
        return render_template('500.html')
    resp = s.recv(500)
    if resp[ :len('OK')] != 'OK':
        print resp
        s.close()
        return render_template('404.html')
    else:
        url = resp[resp.find('http'): ]
    
    print "get url from rd: %s" % url
    
    # make a response to client
    f = urllib.urlopen(url)
    contents = f.read()
    response = app.make_response(contents)
    change_type(obj, response)
    
    s.close()
    return response
    
"""
GET request from a peer node
"""
@app.route('/rd/<int:local_port>/<path:obj>', methods=["GET"])
def relay_request(local_port, obj):
    app.logger.debug("in relay_request obj is %s\n", obj)
    if(None == obj):
        return render_template('501.html')
    
    """
    This is for the longest prefix match
    """
    if obj[0] == '\\':
        obj = '/' + obj[2:]

    # request to rd
    rd_req = 'GETRD ' + str(len(obj)) + ' ' + obj
    print 'request to rd: %s' % rd_req
    try:
        s = socket(AF_INET, SOCK_STREAM)
    except Exception:
        print 'Fail to create socket'
        return render_template('500.html')

    try:
        s.connect(("127.0.0.1", local_port))
    except Exception:
        print 'Fail to connect rd'
        return render_template('500.html')

    if (s.send(rd_req) != len(rd_req)):
        print 'error: fail to realy request to rd server.'
        s.close()
        return render_template('500.html')
    resp = s.recv(500)
    if resp[ :len('OK')] != 'OK':
        print resp
        s.close()
        return render_template('404.html')
    else:
        url = resp[resp.find('http'): ]
    
    print "get url from rd: %s" % url

    # make a response to client
    f = urllib.urlopen(url)
    contents = f.read()
    response = app.make_response(contents)
    change_type(obj, response)
    
    s.close()
    return response

def allowed_file(filename):
    return '.' in filename and filename.rsplit('.', 1)[1] in ALLOWED_EXTENSIONS

"""
addfile, POST request
"""
@app.route('/rd/addfile/<int:local_port>', methods=["POST"])
def rd_addfile(local_port):
    print "in rd_addfile"
    app.logger.debug("in rd_addfile obj is %s\n", request.form['object'])
    
    obj_name = request.form['object']
    if(''  == obj_name):
        return render_template('400.html')

    file = request.files['uploadFile']

    if file.filename != '':
        try:
            s = socket(AF_INET, SOCK_STREAM)
        except Exception:
            print 'Fail to create socket'
            return render_template('500.html')

        try:
            s.connect(("127.0.0.1", local_port))
        except Exception:
            print 'Fail to connect rd'
            return render_template('500.html')

        # if the file is already exist, we do not add to rd twice
        rd_req = 'GETRD ' + str(len(obj_name)) + ' ' + obj_name
        if (s.send(rd_req) != len(rd_req)):
            print 'error: fail to send request to rd server.'
            s.close()
            return render_template('500.html')
        resp = s.recv(500)
        if resp[ :len('OK')] == 'OK':
            return render_template('file_already_exist.html')
        
        # not exist, we can add it
        filename = secure_filename(obj_name)
        tmpname = tempfile.mktemp()
        #file.save(os.path.join(app.config['UPLOAD_FOLDER'], obj_name))
        file.save(tmpname)

        hash = hashlib.sha256()
        with open(tmpname, 'r') as f:
            up = f.read(4096)
            while (len(up) > 0):
                hash.update(up)
                up = f.read(4096)

        save_path = app.config['UPLOAD_FOLDER'] + hash.hexdigest()
        shutil.move(tmpname, save_path)    
        
        # request to rd
        req_rd_path = '/' + save_path
        rd_req = 'ADDFILE ' + str(len(obj_name)) + ' ' + obj_name + ' ' + str(len(req_rd_path)) + ' '  + req_rd_path
        print 'request to rd: %s' % rd_req
        s = socket(AF_INET, SOCK_STREAM)
        s.connect(("127.0.0.1", local_port))
        if (s.send(rd_req) != len(rd_req)):
            print 'error: fail to send request to rd server.'
            os.remove(save_path)
            return render_template('500.html')
        resp = s.recv(500)
        if resp[ :len('OK')] != 'OK':
            print resp
            os.remove(save_path)
            print resp
            return resp
    
        s.close()
        return render_template('200.html')
    else:
        print 'client not send the file to us.'
        return render_template('400.html')

# start from here
if __name__ == '__main__':
	if (len(sys.argv) == 2):
		servport = int(sys.argv[1])
		app.config['UPLOAD_FOLDER'] = 'static/'
		app.run(host='0.0.0.0', port=servport, threaded=True, processes=1)
	else:
		print "Usage ./webserver <server-port>\n"
