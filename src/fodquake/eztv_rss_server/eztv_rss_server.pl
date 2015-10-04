#!/usr/bin/perl -w

# Copyright (C) 2010 Jürgen Legler
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
# 
# See the GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

use LWP::Simple;
use XML::RSS;
use Socket;
use IO::Socket;
use IO::Select;
use DBI;

my $s = IO::Socket::INET->new(
				LocalPort =>12000,
				Proto => 'udp') or die "Socket: $@";

my $count = 0;
my $proxies = {};

my $dbargs = {AutoCommit => 0, PrintError => 1};



sub get_proxy
{
	my $host = $_[0];
	$host =~ s/\n//;
	my $dbh = DBI->connect("dbi:SQLite:dbname=proxies.db", "", "", $dbargs);
	($proxy_id, $id) = $dbh->selectrow_array("SELECT proxy_id,id FROM servers WHERE name=\'" . $host . "\';");
	print "SELECT proxy_id,id FROM servers WHERE name=\'" . $host . "\';\n";
	if ($dbh->err()) { return "error";}
	if (!defined($id)) { return "error";}
	if (!defined($proxy_id)) { return "error";}
	print $proxy_id  . " " . $id . "\n";
	
	$proxy = $dbh->selectrow_array("SELECT name FROM proxies WHERE id=\'" . $proxy_id . "\';");
	if ($dbh->err()) { return "error";};
	print $proxy . "\n";
	
	return $id . "@" . $proxy;
}

while (1)
{
	$s->recv($newmsg, 4096);
	print "recieved " , $newmsg;
	$msg = get_proxy($newmsg);
	$s->send($msg);
	
}
