# First Class- 11 November

## Task 1- HTTP Connection

```bash
telnet example.com 80
Trying 23.220.75.245...
Connected to example.com.
Escape character is '^]'.
GET / HTTP/1.1
HOST: example.com

HTTP/1.1 200 OK
Content-Type: text/html
ETag: "bc2473a18e003bdb249eba5ce893033f:1760028122.592274"
Last-Modified: Thu, 09 Oct 2025 16:42:02 GMT
Content-Length: 1595
Cache-Control: max-age=86000
Date: Tue, 11 Nov 2025 11:17:11 GMT
Connection: keep-alive

<!doctype html><html lang="en"><head><title>Example Domain</title><meta name="viewport" content="width=device-width, initial-scale=1"><style>body{background:#eee;width:60vw;margin:15vh auto;font-family:system-ui,sans-serif}h1{font-size:1.5em}div{opacity:0.8}a:link,a:visited{color:#348}</style><body><script nonce="8755BB99680">var zphInj="eyJkc3RfaXAiOiIyMy4yMjAuNzUuMjQ1IiwiZHN0X3BvcnQiOiI4MCIsImVuY29kZWRfaHR0cF9ob3N0IjoiWlhoaGJYQnNaUzVqYjIwPSIsImVuY29kZWRfdmlhIjoiIiwiaHR0cF9zdGF0dXMiOiIyMDAiLCJlbmNvZGVkX2h0dHBfc2VydmVyIjoiIiwibWV0aG9kIjoiR0VUIiwiY29udGVudF9sZW5ndGgiOiI1MTMiLCJlbmNvZGVkX2NvbnRlbnRfdHlwZSI6ImRHVjRkQzlvZEcxcyIsImludGVyZmFjZSI6ImJvbmQxLjg5IiwicnVsZV9pZCI6InswM0FCNEUwOC05OUY4LTQ0QjctODk0NC02NUU2N0RGQzcwMzB9IiwiaXNfZXhjZXB0aW9uIjoiMCIsImFjdGlvbl9ieV9leGNlcHRpb24iOiJQUkVWRU5UIiwidHJhY2siOiJsb2ciLCJpbmNpZGVudF9pZCI6Ins4NzU1QkI5OS02ODBGLThEOUQtQjFFMC0zMUYwMEUzODMxNzB9In0="</script><script nonce="8755BB99680" src='http://zero-phishing.iaas.checkpoint.com/zph/token_generator.php?api_key={E7E309FA-27A5-6748-B4FA-F65F8FCF2AAE}' crossorigin></script><script nonce="8755BB99680" src='http://zero-phishing.iaas.checkpoint.com/zph/js/sm.js?api_key={E7E309FA-27A5-6748-B4FA-F65F8FCF2AAE}' crossorigin></script><script nonce="8755BB99680" src='https://zerophishing.iaas.checkpoint.com/3/zp.js?api_key={E7E309FA-27A5-6748-B4FA-F65F8FCF2AAE}' defer crossorigin></script><div><h1>Example Domain</h1><p>This domain is for use in documentation examples without needing permission. Avoid use in operations.<p><a href="https://iana.org/domains/example">Learn more</a></div></body></html>

```

## Task 2 - google.com

```bash
telnet google.com 80
Trying 142.250.200.142...
Connected to google.com.
Escape character is '^]'.
GET / HTTP/1.1
HOST: google.com

HTTP/1.1 301 Moved Permanently
Location: http://www.google.com/
Content-Type: text/html; charset=UTF-8
Content-Security-Policy-Report-Only: object-src 'none';base-uri 'self';script-src 'nonce-uaFOQMiPYVqnK_Wog6RPtw' 'strict-dynamic' 'report-sample' 'unsafe-eval' 'unsafe-inline' https: http:;report-uri https://csp.withgoogle.com/csp/gws/other-hp
Date: Tue, 11 Nov 2025 11:22:42 GMT
Expires: Thu, 11 Dec 2025 11:22:42 GMT
Cache-Control: public, max-age=2592000
Server: gws
Content-Length: 219
X-XSS-Protection: 0
X-Frame-Options: SAMEORIGIN

<HTML><HEAD><meta http-equiv="content-type" content="text/html;charset=utf-8">
<TITLE>301 Moved</TITLE></HEAD><BODY>
<H1>301 Moved</H1>
The document has moved
<A HREF="http://www.google.com/">here</A>.
</BODY></HTML>

```

## Task 3 - FTP example connection

```bash
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
  <title>Debian Archive</title>
  <meta name="Modified" content="2025-09-06">
</head>
<body>

<h1>Debian Archive</h1>

<p>See <a href="https://www.debian.org/">https://www.debian.org/</a>
for information about Debian GNU/Linux.</p>

<h2>Current Releases</h2>

<p>Four Debian releases are available on the main site:</p>

<blockquote>
<dl>

<dt><a href="dists/bullseye/">Debian 11.11, or bullseye</a></dt>
<dd>Debian 11.11 was released Saturday, 31st August 2024.
<a href="https://www.debian.org/releases/bullseye/amd64/">Installation
and upgrading instructions</a>,
<a href="https://www.debian.org/releases/bullseye/">More information</a>
</dd>

<dt><a href="dists/bookworm/">Debian 12.12, or bookworm</a></dt>
<dd>Debian 12.12 was released Saturday, 6th September 2025.
<a href="https://www.debian.org/releases/bookworm/amd64/">Installation
and upgrading instructions</a>,
<a href="https://www.debian.org/releases/bookworm/">More information</a>
</dd>

<dt><a href="dists/bookworm/">Debian 13.1, or trixie</a></dt>
<dd>Debian 13.1 was released Saturday, 6th September 2025.
<a href="https://www.debian.org/releases/trixie/amd64/">Installation
and upgrading instructions</a>,
<a href="https://www.debian.org/releases/trixie/">More information</a>
</dd>

<dt><a href="dists/testing/">Testing, or forky</a></dt>
<dd>The current tested development snapshot is named forky.<br>
Packages which have been tested in unstable and passed automated
tests propagate to this release.<br>
<a href="https://www.debian.org/releases/testing/">More information</a>
</dd>

<dt><a href="dists/unstable/">Unstable, or sid</a></dt>
<dd>The current development snapshot is named sid.<br>
Untested candidate packages for future releases.<br>
<a href="https://www.debian.org/releases/unstable/">More information</a>
</dd>
</dl>
</blockquote>

<h2>Old Releases</h2>

<p>Older releases of Debian are at
<a href="http://archive.debian.org/debian-archive/">http://archive.debian.org/debian-archive</a>
<br>
<a href="https://www.debian.org/distrib/archive">More information</a>
</p>

<h2>CDs</h2>

<p>For more information about Debian CDs, please see
<a href="README.CD-manufacture">README.CD-manufacture</a>.
<br>
<a href="https://www.debian.org/CD/">Further information</a>
</p>

<h2>Mirrors</h2>

<p>For more information about Debian mirrors, please see
<a href="README.mirrors.html">README.mirrors.html</a>.
<br>
<a href="https://www.debian.org/mirror/">Further information</a>
</p>

<h2>Other directories</h2>

<table border="0" summary="Other directories">
<tr><td><a href="doc/">doc</a></td>          <td>Debian documentation.</td></tr>
<tr><td><a href="indices/">indices</a></td>  <td>Various indices of the site.</td></tr>
<tr><td><a href="project/">project</a></td>  <td>Experimental packages and other miscellaneous files.</td></tr>
</table>

</body>
</html>

```

[Website to explain FTP](https://www.omnisecu.com/tcpip/ftp-active-vs-passive-modes.php#google_vignette)
