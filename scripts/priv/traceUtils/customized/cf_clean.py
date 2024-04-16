""" 
this script takes the raw dataset, clean it up, fill in TTL, and generate a cleaned dataset

"""

import os, sys
import time
import csv
import requests
import pickle, json
from inspect import currentframe, getframeinfo
from collections import defaultdict, Counter
import hashlib
import struct
import socket

# import geoip2.database
# ip_database = geoip2.database.Reader("GeoLite2-Country.mmdb")

CONTENT_TYPE_SET = set([
    'jpeg',
    'bin',
    'json',
    'js',
    'png',
    'webp',
    '',
    'ts',
    'css',
    'html',
    'm3u8',
    'svg',
    'gif',
    'empty',
    'dat',
    'mp4',
    'xml',
    'txt',
    'ors',
    'woff',
    'bundle',
    'woff2',
    'tgz',
    'jpg',
    'cer',
    'plist',
    'mpga',
    'ico',
    'pbf',
    'ccbi',
    'qt',
    'ttf',
    'webm',
    'tar',
    'zip',
    'ics',
    'rss',
    'aac',
    'pdf',
    'conda',
    'eot',
    'debounce',
    'crl',
    'm4s',
    'enc',
    'otf',
    'wav',
    'm4a',
    'flat',
    'atlas',
    'der',
    'wys',
    'oga',
    'gz',
    'manifest',
    'get',
    'mp3',
    'php',
    'swf',
    'skel',
    'jar',
    'template',
    'odft',
    'bmp',
    'apk',
    'rpm',
    'xz',
    'mpd',
    'io',
    'btx',
    'fnt',
    'xhtml',
    'pub',
    'exe',
    'unityweb',
    'csv',
    'mat',
    'events',
    'ogx',
    'geo',
    'docx',
    'sprite',
    'micro',
    'mid',
    'jp2',
    'doc',
    'identity',
    'vtt',
    'terrain',
    'xlsx',
    'atom',
    'epub',
    'chunk',
    'si d',
    'm3u',
    '7z',
    'invert',
    'findkey',
    'axd',
    'xls',
    'db',
    'mpf',
    'lua',
    'webvtt',
    'unity3d',
    'appcache',
    'tiff',
    'rtf',
    'wasm',
    'jpgx',
    'gzip',
    'md',
    'ppt',
    'lb',
    'ashx',
    'm4v',
    'py',
    'srt',
    'psd',
    'settings',
    'dmg',
    'asp',
    'sh',
    'aspx',
    'patch',
    'dib',
    'mov',
    'rar',
    'last',
    'gcfg',
    'yml',
    'atf',
    'java',
    'stl',
    'chm',
    'crc',
    'yaml',
    'ai',
    'c',
    'unity',
    'srx',
    'scss',
    'includes',
    'flac',
    'caf',
    'extra',
    'xslt',
    'gpg',
    'rdf',
    'bam',
])
EXTENSION_SET = set([
    'jpg', '', 'tgz', 'js', 'png', 'json', 'm3u8', 'ts', 'css', 'webp', 'jpeg',
    'svg', 'php', 'gif', 'mp4', 'dat', 'woff2', 'html', 'bundle', 'woff',
    'rss', 'axd', 'crt', 'plist', 'mp3', 'txt', 'ico', 'pbf', 'gz', 'mov',
    'ttf', 'm4s', 'ccbi', 'bin', 'lb', 'gzip', 'aspx', 'webm', 'jpgx', 'bz2',
    'xml', 'zip ', 'ics', 'aac', 'pdf', 'conda', 'asp', 'ashx', 'eot', 'do',
    'htm', 'atlas', 'crl', 'id', 'otf', 'map', 'bam', 'lua', 'xsd', 'erik',
    'fca', 'enc', 'rpoly', 'properties', 'wav', 'h', 'flat', 'ogg', 'wys',
    'manifest', 'm4a', 'jar', 'ece', 'gmp', 'dds', 'sig', 'pk2mz', 'swf',
    'debounce', 'skel', 'template', 'acz', 'pack', 'cdiff', 'cson', 'jsonp',
    'hash', 'dmg', 'sml', 'apk', 'lzma', 'jsp', 'rpm', 'cdf', 'io', 'csv',
    'xz', 'tga', 'mpd', 'sha256', 'fnt', 'btx', 'depmap', 'distro', 'unity3d',
    'svga', 'omit', 'xhtml', 'vipr', 'uniq', 'res', 'pub', 'b3dm', 'exe',
    'fit', 'ln4', 'crop', 'pairs', 'prefab', 'pl', 'mkv', 'mat', 'cef', 'wmt',
    'bif', 'dxt', 'vtt', 'deb', 'lm', 'geo', 'py', 'sprite', 'vghddata',
    'docx', 'jfif', 'nfs', 'js', 'gifv', 'dlt', 'mid', 'data', 'unityweb',
    'cms', 'jp2', 'identity', 'gcd', 'img', 'bmp', 'doc', 'cur', 'ect', 'page',
    'pic', 'db', 'mjs', 'tif', 'meta', 'image', 'faces', 'prop', 'dll', 'xlsx',
    'pfx', 'box', 'ani', 'chunk', 'terrain', 'epub', '7z', 'jnx', 'midi',
    'tfl', 'asr', 'act', 'xrf', 'mpf', 'ln3', 'ejs', 'lani', 'avif', 'sh',
    'inc', 'vue', 'xaf', 'webvtt', 'pptx', 'aff', 'wasm', 'flow', 'jmm',
    'atom', 'ovpn', 'log', 'so', 'xpf', 'xls', 'anm', 'pngx', 'cmfv', 'gaf',
    'aiu', 'srt', 'hvm', 'dwg', 'yml', 'mem', 'mobile', 'cvd', '3ds', 'java',
    'lmat', 'md', 'sha1', 'm4v', 'tar', 'vbs', 'msi', 'rtf', 'svgz',
    'appcache', 'psd', 'tmx', 'eps', 's3d', 'vpk', 'ini', 'stl', 'link',
    'shtml', 'ppt', 'pkg', 'br', 'ttc', 'patch', 'dib', 'gcfg', 'yaml', 'atf',
    'app', 'heic', 'lyric', 'simple', 'rss2', 'ebr', 'unity', 'rar', 'rgx',
    'obj', 'md2', 'chm', 'crc', 'trf', 'gpi', 'lib', 'jpe', 'scss', 'xsl',
    'pkmlz', 'cgi', 'srx', 'mdr'
    'stat', 'sqlite', 'tiff', 'flac', 'sep', 'caf', 'mps', 'tdb', 'jpeg',
    'cfm', 'gpg', 'geojson', 'sql', 'scml', 'gtt', 'bat', 'c', 'xmf', 'gsm',
    'fsp', 'gcode', 'gifx', 'odt', 'opus', 'rbxm', 'gl', 'apkm', 'pak', 'util',
    'cr2', 'conf', 'dylib', 'dict', 'm3u', 'cer', 'cpp', 'md5', 'xlsm', 'tsx',
    'javascript'
])


def processing(datafile, sample_ratio=1):
    ifile = open(datafile, errors="ignore")
    reader = csv.reader(ifile, delimiter="\t")

    ofilename = datafile
    if sample_ratio > 1:
        ofilename = "{}.sample{}".format(datafile, sample_ratio)

    ofile = open(ofilename + ".clean", "w")
    ofilebin = open(ofilename + ".cf.bin", "wb")
    # ts, obj, sz, ttl, age, hostname, content (h), extension (h), n_level, n_param, method, colo
    # 41 bytes
    s = struct.Struct("<IQQiiihhhbbb")

    ttl_dict = {}
    colo = 28
    content_mapping = defaultdict(int)
    extension_mapping = defaultdict(int)
    hostname_mapping = defaultdict(int)

    content_cnt = Counter()
    extension_cnt = Counter()
    hostname_cnt = Counter()
    method_cnt = Counter()
    n_level_cnt = Counter()
    n_param_cnt = Counter()

    with open("ttl_dict.pickle", "rb") as f:
        # for allcolo data, we do not have content_type,
        # and we use convertTTLDict.py to convert ttl_dict to a different format
        # with open("ttl_dict_new.pickle", "rb") as f:
        ttl_dict = pickle.load(f)

    n = 0
    n_no_ttl, n_use_ttl_dict = 0, 0
    no_age = 0
    n_time_change = 0
    last_ts = 0

    for i in range(1000000):
        # skip the first 1mm requests due to logging issues
        next(reader)

    for row in reader:
        # ts            country      obj                  sz     content tieredHit      ttl             age    cstatus    method     zoneId

        # colo28
        # ts                    obj                sz     content tier      ttl             age         cstatus  method
        # 1628639938      14769378147408796115    8688    jpeg    0       2666327358      39582683        miss    GET     s3.amazonaws.com        /a.pinterest.com/b/c/d/e/f.jpg

        # all colo
        # ts             client         colo        obj                    sz    tier    cStatus
        # 626591102      1433663661      87      10824946938601195600    2944141 0       hit     a.b.com  /production/uploading/c/d/master.mp4

        n += 1
        try:
            ts, obj, sz, content_type, tieed_hit, ttl, age, cstatus, method, hostname, path, query_string = row

            # for allcolo data, some information is missing
            # ts, client, colo, obj, sz, tiered_hit, cstatus, hostname, path, query_string = row
            # ttl, age, content_type, method = 86400*365*100, 86400*365*100, "", "GET"
            # colo = int(colo)
            # client = socket.inet_ntop(socket.AF_INET, struct.pack("!L", int(client)))
            # client_country = ip_database.country(client).country.iso_code
        except Exception as e:
            print(e, row)
            continue

        if sample_ratio > 1 and int(obj) % sample_ratio != 1:
            continue

        ts, ttl, age = int(ts), int(ttl), int(age)
        if ts < last_ts:
            # ts = last_ts
            n_time_change += 1
        last_ts = ts

        # hostname = str(int(hashlib.md5(hostname.encode()).hexdigest(), 16) % (2**30))

        if ttl > 86400 * 365 and (hostname, content_type) in ttl_dict:
            ttl = int(ttl_dict[(hostname, content_type)])
            n_use_ttl_dict += 1

        has_query_string = len(query_string.strip()) > 0
        if has_query_string:
            has_query_string = 1
        else:
            has_query_string = 0

        n_param = query_string.count("&") + 1
        n_level = path.count("/") - 1

        content_type = content_type.lower()
        extension = path.split(".")[-1].lower()
        if '=' in extension:
            extension.split("=")[0]
        if '%' in extension:
            extension.split("%")[0]
        if '@' in extension:
            extension.split("@")[0]

        if "-" in extension or "/" in extension or "_" in extension or len(
                extension
        ) > 10 or extension == "unknown" or extension == "empty":
            extension = ""

        if extension.isdigit():
            extension = ""

        extension = extension.strip()

        if content_type not in CONTENT_TYPE_SET:
            content_type = ""

        if extension not in EXTENSION_SET:
            extension = "unknown"

        # for allcolo data, we do not have content_type
        # if ttl > 86400 * 365 * 30:
        #     if hostname in ttl_dict:
        #         if extension in ttl_dict[hostname]:
        #             ttl = int(ttl_dict[hostname][extension])
        #         else:
        #             ttl = int(list(ttl_dict[hostname].values())[0])
        #         n_use_ttl_dict += 1
        #     else:
        # n_no_ttl += 1
        #         ttl = -1

        if ttl > 86400 * 365 * 30:
            n_no_ttl += 1
            ttl = -1

        if age > 86400 * 365 * 30:
            no_age += 1
            age = -1

        ofile.write(",".join([
            str(ts), obj,
            str(sz), content_type, extension,
            str(n_level),
            str(ttl),
            str(age), cstatus, method, hostname,
            str(has_query_string),
            str(n_param)
        ]) + "\n")

        if n % 20000000 == 0:
            # print(sorted(content_cnt.items(), key=lambda x:x[1], reverse=True))
            # print(sorted(extension_cnt.items(), key=lambda x:x[1], reverse=True))
            print(sorted(method_cnt.items(), key=lambda x: x[1], reverse=True))
            # print(sorted(n_level_cnt.items(), key=lambda x:x[1], reverse=True))
            # print(sorted(n_param_cnt.items(), key=lambda x:x[1], reverse=True))
            print(
                "{:.0f} {} req, {} missingTTL, {} replaceTTL, {} noAge, {} timeDiff"
                .format(time.time(), n // 1e6, n_no_ttl // 1e6,
                        n_use_ttl_dict // 1e6, no_age // 1e6, n_time_change))

        content_int = content_mapping.get(content_type,
                                          len(content_mapping) + 1)
        extension_int = extension_mapping.get(extension,
                                              len(extension_mapping) + 1)
        hostname_int = hostname_mapping.get(hostname,
                                            len(hostname_mapping) + 1)
        content_mapping[content_type] = content_int
        extension_mapping[extension] = extension_int
        hostname_mapping[hostname] = hostname_int

        method_int = 0
        if method == "GET":
            method_int = 1
        elif method == "PURGE":
            method_int = 2
        else:
            print(f"unknown method {method}")

        if has_query_string == 0:
            n_param = 0

        if n_level > 127:
            n_level = 127

        if n_param > 127:
            n_param = 127

        # ts, obj, sz, ttl, age, hostname, content (h), extension (h), n_level (b), n_param (b), method (b), colo (b)
        try:
            ofilebin.write(
                s.pack(int(ts), int(obj), int(sz), ttl, age, hostname_int,
                       content_int, extension_int, colo, n_level, n_param,
                       method_int))
        except Exception as e:
            print(e)
            print(row)
            print(int(ts), int(obj), int(sz), ttl, age, hostname_int,
                  content_int, extension_int, n_level, n_param, method_int,
                  colo)
            print(",".join([
                str(ts), obj,
                str(sz), content_type, extension,
                str(n_level),
                str(ttl),
                str(age), cstatus, method, hostname,
                str(has_query_string),
                str(n_param)
            ]) + "\n")

        content_cnt[content_type] += 1
        extension_cnt[extension] += 1
        hostname_cnt[hostname] += 1
        method_cnt[method] += 1
        n_level_cnt[n_level] += 1
        n_param_cnt[n_param] += 1

    ifile.close()
    ofile.close()
    ofilebin.close()

    with open("{}_stat.json".format(ofilename), "w") as ofile:
        json.dump((content_cnt, extension_cnt, hostname_cnt, method_cnt,
                   n_level_cnt, n_param_cnt), ofile)

    with open("{}_content_mapping.json".format(ofilename), "w") as ofile:
        json.dump(content_mapping, ofile)
    with open("{}_extension_mapping.json".format(ofilename), "w") as ofile:
        json.dump(extension_mapping, ofile)
    with open("{}_hostname_mapping.json".format(ofilename), "w") as ofile:
        json.dump(hostname_mapping, ofile)


if __name__ == "__main__":
    processing(sys.argv[1])
