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
use DBI;
use Socket;
use XML::RSS;

my $dbargs = {AutoCommit => 1, PrintError => 1};
my $dbh = DBI->connect("dbi:SQLite:dbname=proxies.db", "", "", $dbargs);
$dbh->do("DELETE FROM proxies;");
$dbh->do("DELETE FROM servers;");

my $count = 0;
my $proxies = {};


sub get_rss_data
{
	$url = $_[0];
	$url =~ /http:\/\/(.*)\//;

	my $rss = XML::RSS->new();
	my $data = get($url);
	if (!defined($data))
	{
		print "error\n";
		$count--;
		return;	
	}
	$rss->parse($data);


	
	$dbh->do("INSERT INTO proxies (name) VALUES (\'" . $1 . "\');");
	if ($dbh->err()) { die "$DBI::errstr\n"};
	
	$id = $dbh->selectrow_array("SELECT id FROM proxies WHERE name=\'" . $1 . "\';");
	if ($dbh->err()) { die "$DBI::errstr\n"};

	print $id . "\n";

	my $internal_count = 1;
	foreach my $item (@{$rss->{items}})
	{
		$$item{title} =~ /(.*):(\d+)/;
		$iaddr = inet_aton($1);
		if (defined($iaddr))
		{
			$address = inet_ntoa($iaddr);
			$dbh->do("INSERT INTO servers (proxy_id, id, name) VALUES (\'" . $id . "\', \'" . $internal_count . "\', \'" . $address . ":" . $2 . "\');");
			if ($dbh->err()) { die "$DBI::errstr\n"};
		}
		$internal_count++;
#		$addresses[$int_count] = $address . ":" . $2;
	}
}

sub update_db
{
	print "updating db\n";
	$count = 0;
	get_rss_data("http://butt.se:30000/rss");
	$count++;
	get_rss_data("http://quakeworld.fi:28000/rss");
	$count++;
	get_rss_data("http://qtv.quakeservers.net:30000/rss");
	$count++;
}


update_db();

