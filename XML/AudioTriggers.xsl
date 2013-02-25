<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="html"/>

<xsl:template match="/audiotriggers">
	<html>
	<head>
	<style type="text/css">
@charset "utf-8";
/* CSS Document */

table a:link {color: #CC6633;}
table a:visited {color: #CC6633;}
table a:hover {color: #999966;}
table a:active {color: #CC6633;}
table a:focus{color: #CC6633;}


table caption {
		padding: 18px 2px 15px 2px;
		color: #303030;
		background-color: inherit;
		font-weight: 900;
		text-align: center;
		text-transform: capitalize;
		}

	table{
		border: 1px solid #D9D9D9;
	}
	table tr td{
		padding: 6px 9px;
		text-align:left;

	}
	table thead th{
		background-color: #E5E5D8;
		border-bottom: 1px solid #ccc;
		border-left: 1px solid #D9D9D9;
		font-weight: bold;
		text-align:left;
		padding: 16px 9px;
		color:#592C16;
	}
	table tbody tr th{
		background-color: #fff;
		font-weight: normal;
		border-left: none;
		padding: 6px 9px;
		background-color: #E5E5D8;
	}
	table tbody td{
			border-left: 1px solid #D9D9D9;
	}
	table tbody tr.odd{
		background-color: #F3F3F3;
	}
	
table tbody tr:hover {
	color: #333333;
	background-color: #E5E5D8;
}

table tbody tr.odd:hover {
color: #333333;
	background-color: #E5E5D8;
}
	
	table tfoot td, table tfoot th{
		border-top: 1px solid #ccc;
		font-weight:bold;
		color:#592C16;
		padding: 16px 9px;
		
	}
	</style>
	</head>
	<table>
		<caption>Sounds</caption>
		<thead>
			<tr><th>Sound name</th><th>Sound file</th><th>Minimum<br/>repeat<br/>interval (ms)</th></tr>
		</thead>
		<tbody>
			<xsl:for-each select="sound">
				<xsl:sort select="@name"/>
			<tr>
<td>
<a id="SOUND{@name}"><xsl:value-of select="@name"/></a>
</td>

<td><xsl:value-of select="file"/></td>
<td><xsl:value-of select="min_interval"/></td></tr>
			</xsl:for-each>
		</tbody>
	</table>
	<p/>
	<table>
		<caption>Triggers</caption>
		<thead>
			<tr><th>Trigger name</th><th>Trigger Pattern</th><th>Sound to play</th><th>Comment</th></tr>
		</thead>
		<tbody>
			<xsl:for-each select="trigger">
				<xsl:sort select="@name"/>
		<tr><td><a id="TRIGGER{@name}"><xsl:value-of select="@name"/></a></td><td>"<xsl:value-of select="pattern" disable-output-escaping="yes"/>"</td><td><a href="#SOUND{sound_to_play}"><xsl:value-of select="sound_to_play"/></a></td><td><xsl:value-of select="comment"/></td></tr>
			</xsl:for-each>
		</tbody>
	</table>
	<p/>
	<xsl:for-each select="logfile">
	<table>
		<caption>Log File: <xsl:value-of select="file"/></caption>
		<thead>
			<tr><th>Attach trigger</th><th>Stop on match?</th></tr>
		</thead>
		<tbody>
			<xsl:for-each select="attach_trigger">
			<tr>
			<td><a href="#TRIGGER{@name}"><xsl:value-of select="@name"/></a></td>
			<td><xsl:apply-templates/></td>
			</tr>
			</xsl:for-each>
		</tbody>
	</table>
	</xsl:for-each>
	</html>
</xsl:template>
<xsl:template match="stop_search_on_match">Yes</xsl:template>


</xsl:stylesheet>
