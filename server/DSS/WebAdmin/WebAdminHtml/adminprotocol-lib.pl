# adminprotocol-lib.pl
# Common functions for talking to the admin module of QTSS

package adminprotolib;

# Vital libraries
#use IO::Socket;
use Socket;

# GetData(data, authheader, serverName, port, uri) 
# Does an HTTP GET to a server and puts the body in a scalar variable
# Returns the status code of the response from the server
sub GetData
{ 
    my ($authheader, $remote,$port, $iaddr, $paddr, $proto, $uri);
    $authheader = $_[1];
    $remote = $_[2];
    $port = $_[3];
    $uri = $_[4];

    my $status = 500;
    if(!($iaddr = inet_aton($remote))) {
    	$_[0] = "No host: $remote";
    	return $status;
    }
    $paddr = sockaddr_in($port, $iaddr);
    $proto = getprotobyname('tcp');
    if(!socket(CLIENT_SOCK, PF_INET, SOCK_STREAM, $proto)) {
    	$_[0] = "Socket failed: $!";
    	return $status;
    }
    if(!connect(CLIENT_SOCK, $paddr)) {
     	$_[0] = "Connect failed: $!";
     	close (CLIENT_SOCK);
     	return $status;
 	}
 	
    # send request
    $request = "GET $uri HTTP/1.1\r\nUser-Agent: PerlScript\r\nAccept: */*\r\nConnection: close\r\n" . "$authheader\r\n";
    my $bytesSent = 0;
    while($bytesSent < length($request)) {
		$partOfRequest = substr($request, $bytesSent);
		if(!($bytes = send(CLIENT_SOCK, $partOfRequest, 0))) {
			$_[0] = "Send failed: $!";
			close (CLIENT_SOCK);
			return $status;
		}
		$bytesSent += $bytes;
    }
		 
    # read response
    my $partOfResponse;
    $response = "";
    while(1) {

		$partOfResponse = "";
		#if($^O eq "MSWin32") {
	    #	($servipaddr = recv(CLIENT_SOCK, $partOfResponse, 1024, 0)) || last;
		#}
		#else {
			($numBytesRead = read(CLIENT_SOCK, $partOfResponse, 1024)) || last;
		#}	   
		$response .= $partOfResponse;
    }
        
    # read response headers
    my @lines = split /\n/m, $response; 
    my $line = shift @lines;
  
    # Check the status code of the response
    if ($line =~ m/^(\S*?)(\s)(.*?)(\s)(\S*?)(\s*)$/) {
		$status = $3;
    }
    
   # Go through the rest of the headers
    while(@lines) {
		$line = shift @lines;
		if ($line =~ m/^\s*$/) { last; }
		if($line =~ /^(\S+):\s+(.*)$/) {
			if(lc($1) eq "www-authenticate") {
				$challenge = $line;
			} 
		}
    }
    
    # Read the response body
    if ($status == 200) { 
		$responseText = "";
		while(@lines) {
	   		$line = shift @lines;
	    	$responseText .= "$line\n";
		}
		$_[0] = $responseText;
    }
    elsif ($status == 401) {
    	$_[0] = $challenge;
    }
    
    close (CLIENT_SOCK);
    return $status;
}

# EchoData(data, authheader, serverName, port, uri, param)
# Uses GetData to fetch the uri and parses the value in param=value 
# into a scalar variable.
# Returns the value as a scalar
sub EchoData {
	my $authheader = $_[1];
    my $serverName = $_[2];
    my $serverPort = $_[3];
    my $uri = $_[4];
    my $param = $_[5];
    my $responseText = "";
    my $status = GetData($responseText, $authheader, $serverName, $serverPort, $uri);
    if($status != 200) {
    	$_[0] = $responseText;
		return $status;
    }

    my $paramName = "";
    my $paramValue = "";
    if ($param =~ m/^(.*)\/(\w+)$/) {
		$paramName = $2;
    }
    else {
		$paramName = $param;
    }

    my @lines = split /\n/, $responseText;
    my $line;

    while(@lines) {
		$line = shift @lines;
		if($line =~ m/^$paramName=\"(.*)\"(\s*)$/) {
		    $paramValue = $1;
		}
    }
    if($paramValue eq "") {
		undef($paramValue);
    }
    $_[0] = $paramValue;
    return $status;
}

# MakeArray(text, name, [size]) 
# Parses the scalar text for the container name and 
# returns an array of all the values. If size is given 
# it looks for size number of elements else it finds all
sub MakeArray
{
    my $text = $_[0];
    my $name = $_[1];
    my $size = 0;
    my $count = 0;

    if($_[3]) { $size = $_[3]; }
    
    my @lines = split /\r|\n/, $text;
    my $line;
    my @arr;
    $#arr = $size - 1;   # pre-grow the array if we know its size

    while(@lines) {
		$line = shift @lines;	
#		if($line =~ m/^Container=\"(.*)$name\"(\s*)$/) {
		if($line =~ m/^Container=\"(.*)\"$/) {
		    while(1) {
				$line = shift @lines;
				if($line =~ m/^$count=\"(.*)\"(\s*)$/) {
				    $arr[$count] = $1;
		    		$count++;
		    		if(($size != 0) && ($size == $count)) { last; }
				}
				else { last; }
	    	}
	    	last;
		}
		elsif($line =~ m/^(.*?)=\"(.*?)\"/) {
			$arr[0] = $2;
			last;
    	}
    }
    return \@arr;
}


#sub FormatArray (\@arrName, beginIndex, endIndex, prefix, suffix) 
# Formats the elements of the array by applying the
# prefix and the suffix to each element in the array
# returns the formatted text
sub FormatArray {
    my $arRef = $_[0];
    my @arr = @$arRef;
    my $index = $_[1];
    my $endIndex = ($_[2] == -1) ? $#arr : $_[2];
    my $prefix = $_[3];
    my $suffix = $_[4];
    
    local $responseText= "";
    for($index; $index <= $endIndex; $index++) {
		$responseText .= $prefix.$arr[$index].$suffix;
    }
    return scalar $responseText;
}
 
# sub HasValue (\@arrOfHash, value, ["num" | alpha"])
# Returns 1 if the array contains the value, 0 otherwise. 
sub HasValue {
    my $arRef = $_[0];
    my @arr = @$arRef;
    my $value = $_[1];
    my $type = $_[2];

	if($type eq "num") {
		for($i = 0; $i <= $#arr; $i++) {
			if($arr[$i] == $value) {
				return 1;
			}
		}
	}
	elsif($type eq "alpha") {
		for($i = 0; $i <= $#arr; $i++) {
			if($arr[$i] eq $value) {
				return 1;
			}
		}
	}
	return 0;
}

# sub SetAttribute (data, authheader, server, port, fullpath, value, [type])
# Sends an admin protocol set command and returns the error value
sub SetAttribute {
	my $uri = "/modules".$_[4]."?command=set+value="."\"$_[5]\"";
	my $code = 400;
	if($_[6]) {
		$uri .= "+type=$_[6]";
	}
	my $data = "";
	$status = GetData($data, $_[1], $_[2], $_[3], $uri); 
	if($status == 200) {
		if($data =~ m/^error:\((.*)\)/) {
			$code = $1;
		}
	}
	else {
		$code = $status;
		$_[0] = $data;
	}
	return $code;
}

# sub SetPassword (data, authheader, server, port, fullpath, value)
# Sends an admin protocol set command for the admin password and returns the error value
sub SetPassword {
	my $uri = "/modules".$_[4]."?command=set+value="."\"$_[5]\"";
	my $code = 400;
	my $data = "";
	$status = GetData($data, $_[1], $_[2], $_[3], $uri); 
	if($status == 200) {
		if($data =~ m/^error:\((.*)\)/) {
			$code = $1;
		}
	}
	else {
		$code = $status;
		$_[0] = $data;
	}
	return $code;
}

# sub AddValueToAttribute (data, authheader, server, port, fullpath, value)
sub AddValueToAttribute {
	my $uri = "/modules".$_[4]."?command=add+value="."\"$_[5]\"";
	my $code = 0;
	my $data = "";
	$status = GetData($data, $_[1], $_[2], $_[3], $uri);
	if($status == 200) { 
		if($data =~ m/^error:\((.*?)\)$/) {
			$code = $1;
		}
	}
	else {
		$code = $status;
		$_[0] = $data;
	}
	return $code;
}

# sub DeleteValueFromAttribute (data, authheader, server, port, fullpath, value)
sub DeleteValueFromAttribute {
	my $server = $_[2];
	my $port = $_[3];
	my $fullpath = $_[4];
	my $value = $_[5];
	my $code = 0;
	my $data = "";
	my $status = GetData($data, $_[1], $server, $port, "/modules".$fullpath."/*");
	if($status != 200) {
		$code = $status;
		$_[0] = $data;
		return $code;
	}
	my $arRef = MakeArray($data, $fullpath."/");
	my @arr = @$arRef;
	my $index = -1;
	for($i = 0; $i <= $#arr; $i++) {
		if($arr[$i] eq $value) {
			$index = $i;
			last;
		}
	}
	if($index != -1) {
		my $uri = "/modules".$fullpath."/$index"."?command=del";
		$data = "";
		$status = GetData($data, $_[1], $server, $port, $uri); 
		if($status == 200) {
			if($data =~ m/^error:\((.*?)\)$/) {
				$code = $1;
			}
		}
		else {
			$code = $status; 
			$_[0] = $data;
		}
	}
	return $code;
}
 
# sub ParseFile (data, authheader, server, port, filename, [func], [param], [value]....)
# Parses the file for all the server side includes and processes them
# returns the processed file data in a scalar
# The multiple sets of arguments func, param, and value are for taking 
# some input in the cgi for some server side includes
# return values: 	200 - parsing okay and qtss returned values
#					data returned is the output
#					401 - parsing okay but qtss returned authorization failed
#					data returned is the auth challenge http headers
# 					500 - qtss is not responding
#					data - must be empty
sub ParseFile {
	my $authheader = $_[1];
	my $server = $_[2];
	my $port = $_[3];
	my $filename = $_[4];
	my %funcparam;
	my $fkey;
	my $fvalue;
	for($i = 5; $i <= $#_; $i = $i + 3) {
		$fkey = $_[$i] . ":" . $_[$i+1];
		$fvalue = $_[$i+2];
		$funcparam{$fkey} = $fvalue;
	}
	local (*TEMPFILE, $_);
	# Open the file
	open(TEMPFILE, $filename) or print "Can't open $filename: $!\n";
	# Read the entire file into a buffer and close file handle
	read(TEMPFILE, $_, -s $filename);
	close(TEMPFILE);
	my %varHash = ();
	my $data = "";
	my $status;
	
	# Look for <%% Func param%%> tags
	while(/^(.*?)<%%(.*?)%%>(.*)$/s) {
    	$_[0] .= $1;
    	$_ = $3;    
	    $tag = $2;
   		if($tag =~ m/^ECHODATA\s+(.*)/s) {
			@params = split /\s+/, $1;
			$uri = "/modules/admin".$params[0];
			$data = "";
			$status = EchoData($data, $authheader, $server, $port, $uri, $params[0]);
		    if($status == 401) {
		    	$_[0] = $data;
		    	return $status;
		    }
		    elsif($status == 500) {
		    	$data = "";
		    }
		    $_[0] .= $data;
	   	}
 		elsif($tag =~ m/^GETDATA\s+(.*)/s) {
			#storing the retrieved text in a variable hashed to the name
			@params = split /\s+/, $1;
			$uri = "/modules/admin".$params[1];
			$data = "";
			$status =  GetData($data, $authheader, $server, $port, $uri);
			if($status == 401) {
		    	$_[0] = $data;
		    	return $status;
		    }
		    elsif($status == 500) {
		    	undef($data);
		    }
			$varHash{$params[0]} = $data;
    	}
    	elsif($tag =~ m/^GETVALUE\s+(.*)/s) {
			#extract the value from the retreived text and store in a variable for later use
			@params = split /\s+/, $1;
			$uri = "/modules/admin".$params[1];
			$data = "";
			$status = EchoData($data, $authheader, $server, $port, $uri, $params[1]);
			if($status == 401) {
		    	$_[0] = $data;
		    	return $status;
		    }
		    elsif($status == 500) {
		    	undef($data);
		    }
			$varHash{$params[0]} = $data;
    	}
    	elsif($tag =~ m/^MAKEARRAY\s+(.*)/s) {
			#storing the array reference returned hashed to the name
			@params = split /\s+/, $1;
			if(defined($varHash{$params[2]})) {
				$arRef = MakeArray($varHash{$params[2]}, $params[1]);
				$varHash{$params[0]} = $arRef;
    		}
    		else {
    			undef($varHash{$params[0]});
      		}
    	}
	    elsif($tag =~ m/^HASVALUE\s+(.*)/s) {
			#returning 1 if the value exists in the array, 0 otherwise
			@params = split /\s+/, $1;
			$params[2] = ($params[2] =~ m/^\'(.*)\'/) ? $1 : $params[2];
			if(defined($varHash{$params[1]})) {
				$found = HasValue($varHash{$params[1]}, $params[2], $params[3]);
				$varHash{$params[0]} = $found;
    		}
    		else {
    			undef($varHash{$params[0]});
    		}
    	}
    	elsif($tag =~ m/^IFVALUEEQUALS\s+(.*)/s) {
			#returning 1 if the value exists in the array, 0 otherwise
			@params = split /\s+/, $1;
			$params[2] = ($params[2] =~ m/^\'(.*)\'/) ? $1 : $params[2];
			if(!defined($varHash{$params[1]})) {
				undef($found);
			}
			elsif($varHash{$params[1]} eq $params[2]) {
				$found = 1;
			}
			else { $found = 0; }
			$varHash{$params[0]} = $found;
    	}
    	elsif($tag =~ m/^CONVERTTOLOCALTIME\s+(\S+)/) {
    		my $timeval = $varHash{$1};
    		if(!defined($timeval)) {
				$_[0] .= "";
			}
			else {
				$_[0] .= localtime($timeval/1000);		
    		}
    	}
		elsif($tag =~ m/^ACTIONONDATA\s+(\S+)\s+(\S+)\s+(\S+)\s+\'(.*?)\'(\s*)/s) {
			$refKey = $1;
			if(defined($varHash{$2}) && defined($varHash{$3})) {
				$varHash{$refKey} = eval($varHash{$2} . $4 . $varHash{$3});
			}
			else {
				undef($varHash{$refKey});
			}
		}
		elsif($tag =~ m/^FORMATFLOAT\s+(\S+)/s) {
			if(defined($varHash{$1})) {
				$_[0] .= sprintf "%3.2f", $varHash{$1};
			}
			else {
				$_[0] .= "";
			}
		}
		elsif($tag =~ m/^CONVERTMSECTIMETOSTR\s+(\S+)/) {
			if(defined($varHash{$1})) {
				my $timeStr = ConvertTimeToStr($varHash{$1});
				$_[0] .= $timeStr;
			}
			else {
				$_[0] .= "";
			}
		}
    	elsif($tag =~ m/^MODIFYDATA\s+(\S+?)\s+\'(.*?)\'\s+\'(.*?)\'/s) {
    		$value = $varHash{$1};
    		$condition = $2;
    		$action = $3;
    		if(defined($value)) {
    			$newVal = ModifyData($value, $condition, $action);
    			$_[0] .= $newVal;
    		}
    		else {
    			$_[0] .= "";
    		}
    	}
    	elsif($tag =~ m/^PRINTFILE\s+(\S+?)\s+(\S+)/s) {
    		if(!defined($varHash{$1}) || !defined($varHash{$2})) {
    			#$_[0] .= "Server is not Running. Cannot display file";
    			$_[0] .= "";
    		}
    		else {
    			$_[0] .= GetFile($varHash{$1}, $varHash{$2});
    		}
    	}
    	elsif($tag =~ m/^PRINTHTMLFORMATFILE\s+(\S+?)\s+(\S+)/s) {
    		if(!defined($varHash{$1}) || !defined($varHash{$2})) {
    		    #$_[0] .= qq(<BR><BR><FONT FACE="Arial"><B>Server is not Running. Cannot display file.</B></FONT>);
    			$_[0] .= "";
    		}
    		else {
    			$_[0] .= GetFormattedFile($varHash{$1}, $varHash{$2});
    		}
    	}
    	elsif($tag =~ m/PROCESSFILE\s+(\S+?)\s+(\S+?)\s+(\S+?)\s+(\S+)/s) {
    		my $ref = $1;
    		if(defined($varHash{$2}) && defined($varHash{$2})){
    		   $varHash{$ref} = ProcessFile($varHash{$2}, $varHash{$3}, $4);
    		}
    		else {
    			undef($varHash{$ref});
    		}
    	}
    	elsif($tag =~ m/^HTMLIZE\s+(\S+)/s) {
 			$text = $varHash{$1};
 			if(defined($text)) {
 				$htmlText = HtmlEscape($text);
 				$_[0] .= $htmlText;
 			}
 			else {
 				$_[0] .= "";
 			}
 		}
 		elsif($tag =~ m/^PRINTDATA\s+(\S+)\s+\'(.*?)\'\s+\'(.*?)\'/s) {
 			if(!defined($varHash{$1})) {
 				$_[0] .= "";
 			} elsif( $varHash{$1} == 1) {
 				$_[0] .= $2;
 			} else {
 				$_[0] .= $3;
 			}
 		}
 		elsif($tag =~ m/^CONVERTTOVERSIONSTR\s+(\S+)/s) {
 			if(defined($varHash{$1})) {
 				$_[0] .= ConvertToVersionString($varHash{$1});
 			}
 			else {
 				$_[0] .= "";
 			}
 		}
 		elsif($tag =~ m/^CREATESELECTFROMINPUTWITHFUNC\s+(\S+?)\s+(\S+?)\s+(.*)/s) {
	    	my $name = $1;
	    	$value = $funcparam{"CREATESELECTFROMINPUTWITHFUNC:$name"};
	    	if(defined($value)) {
	    		my $handler = $2;
	    		my $optstr = $3;
	    		my @options = ();
	    		my $i = 0;
	    		
	    		while($optstr =~ m/\'(.*?)\'\s+(.*)/) {
	    			if($value eq $1) {
	    				$options[$i] = "<OPTION SELECTED>". $1;
	    			}
	    			else { $options[$i] = "<OPTION>". $1; }
	    			$optstr = $2;
	    			$i++;
	    		}
	    		if($optstr =~ m/\'(.*?)\'/) {
	    			if($value eq $1) {
	    				$options[$i] = "<OPTION SELECTED>". $1;
	    			}
	    			else { $options[$i] = "<OPTION>". $1; }
	    		}
	    		my $result = qq(<SELECT NAME=") . $name. qq(" onChange=") . $handler. qq(()">);
	    		for($i = 0; $i<=$#options; $i++) {
	    			$result .= "$options[$i]\n";
				}
                 $result .=qq(</SELECT>);
				$_[0] .= $result;
			}
		}
		elsif($tag =~ m/^CREATESELECTFROMINPUT\s+(\S+?)\s+(.*)/s) {
	    	my $name = $1;
	    	$value = $funcparam{"CREATESELECTFROMINPUT:$name"};
	    	if(defined($value)) {
	    		my $optstr = $2;
	    		my @options = ();
	    		my $i = 0;
	    		
	    		while($optstr =~ m/\'(.*?)\'\s+(.*)/) {
	    			if($value eq $1) {
	    				$options[$i] = "<OPTION SELECTED>". $1;
	    			}
	    			else { $options[$i] = "<OPTION>". $1; }
	    			$optstr = $2;
	    			$i++;
	    		}
	    		if($optstr =~ m/\'(.*?)\'/) {
	    			if($value eq $1) {
	    				$options[$i] = "<OPTION SELECTED>". $1;
	    			}
	    			else { $options[$i] = "<OPTION>". $1; }
	    		}
	    		my $result = qq(<SELECT NAME=") . $name. qq(">);
	    		for($i = 0; $i<=$#options; $i++) {
	    			$result .= "$options[$i]\n";
				}
                 $result .=qq(</SELECT>);
				$_[0] .= $result;
			}
		}
		elsif($tag =~ m/^SORTRECORDSWITHINPUT\s+(\S+?)\s+(\S+?)\s+(\S+?)\s+(\S+)/s) {
    		my $refKey = $1;
    		$value = $funcparam{"SORTRECORDSWITHINPUT:sortOrder"};
    		if(defined($value) && defined($varHash{$2})) { 
    			$varHash{$refKey} = SortRecords($varHash{$2}, $3, $4, $value);
    		}
		}	
		elsif($tag =~ m/^IFREMOTE\s+\'(.*?)\'\s+\'(.*)\'/s) {
    		my $remoteData = $1;
    		my $localData = $2;
    		$value = $funcparam{"IFREMOTE:ipaddress"};
    		if($value ne '127.0.0.1') { 
    			$_[0] .= $remoteData;
    		}
    		else {
    			$_[0] .= $localData;
    		}
		}
 		elsif($tag =~ m/^CONVERTTOSTATESTR\s+(\S+)/s) {
 			$_[0] .= GetServerStateString($varHash{$1});
 		}
    	elsif($tag =~ m/^FILTERSTRUCT\s+(.*)/s) {
			#filter the elements of the data structure
			@params = split /\s+/, $1;
			$params[3] = ($params[3] =~ m/^\'(.*)\'/) ? $1 : $params[3];
			if(defined($varHash{$params[1]})) {
				$arRef = FilterStructArray($varHash{$params[1]}, $params[2], $params[3]);
				$varHash{$params[0]} = $arRef;
    		}
    		else {
    			undef($varHash{$params[0]});
    		}
    	}
    	elsif($tag =~ m/^FORMATDATA\s+(\S+?)\s+\'(.*?)\'\s+'(.*?)\'\s+'(.*?)\'\s+'(.*?)\'\s+'(.*?)\'/s) {
    		if(defined($varHash{$1})) {
    			$formatValue = FormatData($varHash{$1}, $2, $3, $4, $5, $6); 
    			$_[0] .= $formatValue;
    		}
    		else {
    			$_[0] .= "";
    		}
    	}
    	elsif($tag =~ m/^FORMATRADIOBUTTON\s+(\S+)\s+\'(.*?)\'\s+\'(.*?)\'\s+(\S+)/s) {
    		$cond = $varHash{$1};
    		if(defined($cond)) {
	    		$formatData = FormatRadioButton($2, $3, $cond, $4);
	    		$_[0] .= $formatData;
    		}
    		else {
    			$_[0] .= "";
    		}
    	}
    	elsif($tag =~ m/^FORMATSUBMITBUTTON\s+(\S+?)\s+\'(.*?)\'\s+\'(.*?)\'\s+\'(.*?)\'\s+\'(.*?)\'/s) {
    		$expr = $varHash{$1};
    		$name = $2;
    		$cond = $3;
    		if(eval($expr . $3)) {
    			$value = $4;
    		}
    		else {
    			$value = $5;
    		}
    		$_[0] .= qq(<INPUT TYPE="submit" NAME="$name" VALUE ="$value">);
    	}
    	elsif($tag =~ m/^FORMATSELECTOPTION\s+(\S+)\s+\'(.*?)\'\s+\'(.*?)\'\s+\'(.*?)\'\s+\'(.*?)\'\s+(\S+)/s) {
    		$value = $varHash{$1};
    		$selectName = $2;
    		$condition = $3;
    		$opt1 = $4;
    		$opt2 = $5;
    		$selectOpt = $6;
    		$formatData = FormatSelectOption($value, $condition, $selectName, $opt1, $opt2, $selectOpt);
    		$_[0] .= $formatData;
    	}
	    elsif($tag =~ m/^FORMAT\s+(\S+)\s+(\S+)\s+(\S+)\s+\'(.*?)\'\s+\'(.*?)\'/s) {
			#format the elements of the array
			$arRef = $varHash{$1};
			$formatText = FormatArray($arRef, $2, $3, $4, $5);
			$_[0] .= $formatText;
	    }	
    	elsif($tag =~ m/^DEFINEINDEXHASH\s+(.*)/s) {
    		@params = split /\s+|\r|\n/, $1;
			$hRef = {};
			$hRefKey = shift @params;
			for($i=0;$i<=$#params;$i++) {
				$hRef->{$params[$i]} = ($i + 1);
			}
			$varHash{$hRefKey} = $hRef;
    	}
    	elsif($tag =~ m/^GETALLRECORDS\s+(\S+?)\s+(\S+?)\s+(\S+)/s) {
    		my $arKey = $1;
    		$data = "";
    		$status = GetAllRecords($data, $authheader, $server, $port, $varHash{$2}, $3);
    		if($status == 401) {
    			$_[0] = $data;
    			return $status;
    		}
    		$varHash{$arKey} = $data;
    	}
    	elsif($tag =~ m/^MODIFYCOLUMNWITHCONDS\s+(\S+?)\s+(\S+?)\s+(\S+?)\s+(\S+?)\s+(\S+?)\s+\'(.*?)\'\s+\'(.*?)\'\s+\'(.*?)\'\s+\'(.*?)\'\s+\'(.*?)\'/s) {
    		my $refKey = $1;
    		$varHash{$refKey} = ModifyColumnWithConds($varHash{$2}, $3, $4, $5, $6, $7, $8, $9, $10);
    	}
    	elsif($tag =~ m/^CONVERTCOLUMNTOSTRTIME\s+(\S+?)\s+(\S+?)\s+(\S+?)\s+(\S+?)\s+(\S+)/s) {
    		my $refKey = $1;
    		$varHash{$refKey} = ConvertColumnToStrTime($varHash{$2}, $3, $4, $5);
    	}
    	elsif($tag =~ m/^SORTRECORDS\s+(\S+?)\s+(\S+?)\s+(\S+?)\s+(\S+?)\s+(\S+)/s) {
    		my $refKey = $1;
    		$varHash{$refKey} = SortRecords($varHash{$2}, $3, $4, $5);
    	}
    	elsif($tag =~ m/FORMATDDARRAYWITHINPUT\s+(\S+?)\s+(\S+?)\s+\'(.*?)\'\s+\'(.*?)\'\s+(.*)/s) {
			my $dArrRef = $varHash{$1};
			my @da = @$dArrRef;
			my $begIndex = $2;
			my $endIndex = -1;
			$value = $funcparam{"FORMATDDARRAYWITHINPUT:numEntries"};
    		if(defined($value)) { 
    			$endIndex = $value;
    		}
			my $begRowFormat = $3;
			my $endRowFormat = $4;
			my @formatArr = ();
			my $format = $5;
			my $i = 0;
			while ($format =~ m/^(\S+)\s+\'(.*?)\'\s+\'(.*?)\'\s+(.*)/s) {
			    $formatArr[$i] = $1;
			    $formatArr[++$i] = $2;
			    $formatArr[++$i] = $3;
			    $format = $4;
			    $i++;
			}
			if($format =~ m/^(\S+)\s+\'(.*?)\'\s+\'(.*?)\'(\s*)/s) {
				$formatArr[$i] = $1;
				$formatArr[++$i] = $2;
				$formatArr[++$i] = $3;
			}
			$formatText = FormatDDArray($dArrRef, $begIndex, $endIndex, $begRowFormat, $endRowFormat, @formatArr);
			$_[0] .= $formatText;
		}
		elsif($tag =~ m/FORMATDDARRAY\s+(\S+?)\s+(\S+?)\s+(\S+?)\s+\'(.*?)\'\s+\'(.*?)\'\s+(.*)/s) {
			my $dArrRef = $varHash{$1};
			my @da = @$dArrRef;
			my $begIndex = $2;
			my $endIndex = $3;
			my $begRowFormat = $4;
			my $endRowFormat = $5;
			my @formatArr = ();
			my $format = $6;
			my $i = 0;
			while ($format =~ m/^(\S+)\s+\'(.*?)\'\s+\'(.*?)\'\s+(.*)/s) {
			    $formatArr[$i] = $1;
			    $formatArr[++$i] = $2;
			    $formatArr[++$i] = $3;
			    $format = $4;
			    $i++;
			}
			if($format =~ m/^(\S+)\s+\'(.*?)\'\s+\'(.*?)\'(\s*)/s) {
				$formatArr[$i] = $1;
				$formatArr[++$i] = $2;
				$formatArr[++$i] = $3;
			}
			$formatText = FormatDDArray($dArrRef, $begIndex, $endIndex, $begRowFormat, $endRowFormat, @formatArr);
			$_[0] .= $formatText;
		}
	}
	$_[0] .= $_;
	return $status;
}

# sub GetAllRecords($data, $authheader, $server, $port, \%IndexHash, $requestStr)
sub GetAllRecords {
	my $authheader = $_[1];
	my $uri = "/modules/admin".$_[5]."*?command=get+";
	my $hRef = $_[4];
	my $filterstr;
	my $num = 1;
	foreach $filterstr (keys %$hRef) {
		$uri .= "filter$num=$filterstr+";
		$num++;
	}
	my $data = "";
	my $status = GetData($data, $authheader, $_[2], $_[3], $uri);		
	if($status != 200) {
		$_[0] = $data;
		return $status;
	}
	my @lines = split /\r|\n/, $data;	
	my @ddArr;
	my $i, $j, @elem;
	my @indexArr;
	
	if($lines[0] =~ m/^Container=\".*?\/(\w+)\/\"/) {
		$ddArr[0]->[0] = $1;
		for($j = 0; $j < ($num-1) ; $j++) {
			if($lines[$j + 1] =~ m/^(.*?)=\"(.*?)\"\s*$/) {
				my $ind = $hRef->{$1};
				$indexArr[$j] = $ind;
				$ddArr[0]->[$ind] = $2;
	    	}
		}
	}
	
	$k = 0;
	for($i = $num; $i <= $#lines; $i += $num) {
		if($lines[$i] =~ m/^Container=\".*?\/(\w+)\/\"/) {
			$k++;
			$ddArr[$k]->[0] = $1;
			for($j = 0; $j < ($num-1); $j++) {
				if($lines[$i + $j + 1] =~ m/^(.*?)=\"(.*?)\"\s*$/) {
					$ddArr[$k]->[($indexArr[$j])] = $2;
	    		}
			}
		}
	}
	$_[0] = \@ddArr;
	return $status;		
}


# sub GetAllRecords_WithoutFilters($data, $authheader, $server, $port, \%IndexHash, $requestStr)
sub GetAllRecords_WithoutFilters {
	my $data = "";
	my $status = GetData($data, $_[1], $_[2], $_[3], "/modules/admin".$_[5]."*");
	if($status != 200) {
		$_[0] = $data;
		return $status;
	}
	
	my @lines = split /\n/, $data;
	my @ddArr;
	my $hRef = $_[4];
	my $i = 0;
	
	my $line = shift @lines;
	while(@lines) {
		if($line =~ m/^Container=\".*?\/(\w+)\/\"/) {
			$ddArr[$i] = ();
			$ddArr[$i]->[0] = $1;
			while(@lines) {
				$line = shift @lines;
				if($line =~ /^Container=.*$/) {
		    		last;
				}
				if($line =~ m/^(.*?)=\"(.*?)\"\s*$/) {
					my $ind = $hRef->{$1};
					if(defined($ind)) {
						$ddArr[$i]->[$ind] = $2;
		    		}
				}
			}
			$i++;
		}
	}
	$status = \@ddArr;
	return $status; 	
}

# sub ModifyColumnWithConds(\@dArrRef, $column, $begIndex, $endIndex, $cond, $truAction, $truSuffix, $falAction, $falSuffix)
sub ModifyColumnWithConds {
	my ($dArrRef, $col, $b, $e, $cond, $truAct, $truSuf, $falAct, $falSuf) = @_;
	my @dArr = @$dArrRef;
	my @newDArr = ();
	$e = ($e == -1)? $#dArr : $e;
	my $i;
	for($i = $b; $i <= $e; $i++) {
		my $arRef = $dArr[$i];
		my @newAr = @$arRef;
		my $colValue = $newAr[$col];
		if(eval($colValue . $cond)) {
			$colValue = int(eval($colValue . $truAct)) . $truSuf;
		}
		else {
			$colValue = int(eval($colValue . $falAct)) . $falSuf;
		}
		$newAr[$col] = $colValue;
		$newDArr[$i] = \@newAr;
	}
	return \@newDArr;
}

# sub ConvertColumnToStrTime(\@dArrRef, $col, $begIndex, $endIndex)
sub ConvertColumnToStrTime {
	my ($dArrRef, $col, $b, $e) = @_;
	my @dArr = @$dArrRef;
	my @newDArr = ();
	$e = ($e == -1)? $#dArr : $e;
	my $i;
	for($i = $b; $i <= $e; $i++) {
		my $arRef = $dArr[$i];
		my @newAr = @$arRef;
		$newAr[$col] = ConvertTimeToStr($newAr[$col]);
		$newDArr[$i] = \@newAr;
	}
	return \@newDArr;
}
 
 
sub byAlphaAscending {
    $a->[0] cmp $b->[0];
}

sub byAlphaDescending {
    $b->[0] cmp $a->[0];
}

sub byNumberAscending {
    $a->[0] <=> $b->[0];
}

sub byNumberDescending {
    $b->[0] <=> $a->[0];
} 
 
# sub SortRecords(\@dArrRef, $index, [num|alpha], [asc|Ascending|desc|Descending])
sub SortRecords {
	my ($dArrRef, $index, $type, $order) = @_;
	my @dArr = @$dArrRef;
	my @sortArr = ();
	my $i;
	for($i = 0; $i <= $#dArr; $i++) {
		my $ar = ();
		$ar->[0] = $dArr[$i][$index];
		$ar->[1] = $dArr[$i];
		$sortArr[$i] = $ar;
	}
	
	my @sortedArr;
	if($type eq "num") {
		if(($order eq "asc") || ($order eq "Ascending")) {
			@sortedArr = sort byNumberAscending @sortArr;
		} elsif(($order eq "desc") || ($order eq "Descending")) {
			@sortedArr = sort byNumberDescending @sortArr;	
		}
	} elsif($type eq "alpha") {
		if(($order eq "asc") || ($order eq "Ascending")) {
			@sortedArr = sort byAlphaAscending @sortArr;
		} elsif(($order eq "desc") || ($order eq "Descending")) {
			@sortedArr = sort byAlphaDescending @sortArr;	
		}
	}
	my @resultArr;
	for($i = 0; $i <= $#sortedArr; $i++) {
		$resultArr[$i] = $sortedArr[$i][1];
	}
	return \@resultArr;
}

# sub FormatDDArray(\@dArrRef, $begIndex, $endIndex, $begRowFormat, $endRowFormat, @formatArr)
sub FormatDDArray {
	my $dArrRef = shift @_;
	my @dArr = @$dArrRef;
	my $r, $dumbR, $dumbA;
	my $b = shift @_;
	my $e = shift @_;
	my $e = ($e == -1)? $#dArr : $e;
	my $bformat = shift @_;
	my $eformat = shift @_;
	my @formatArr = @_;
	my %prefix;
	my %suffix;
	my $i = 0;
	while($i<=$#formatArr) {
		my $key = $formatArr[$i];
		$prefix{$key} = $formatArr[++$i];
		$suffix{$key} = $formatArr[++$i];
		$i++;
	}
	my $result = "";
	# if the end exceeds the array size, cut it down
	if($e > $#dArr) {
		$e = $#dArr;
	}
	for($i=$b; $i <= $e; $i++) {
		my $arRef = $dArr[$i];
		$result .= $bformat;
		for $prekey (sort keys %prefix) {
			$result .= $prefix{$prekey} . ($arRef->[$prekey]) . $suffix{$prekey};
		}
		$result .= $eformat;
	}
	return $result;
}

# sub ModifyData(value, condition, action)
sub ModifyData {
	$oldVal = $_[0];
	$expr = $oldVal . $_[1];
	$result = $oldVal . $_[2];
	if(eval($expr)) {
		return (int(eval($result)));
	}
	else {
		return $oldVal;
	}
}

# sub GetFile($dirname, $filename) 
sub GetFile {
	my ($dirname, $filename) = @_;
	my $path = $dirname . qq(/) . $filename . ".log";
	my ($line, $text);
	$text = "";
	open(FILE, $path) or print "Can't open $path: $!\n";
	while($line = <FILE>) {
		$text .= $line;
	}
	close(FILE);
	return $text;
}

# sub GetFormattedFile($dirname, $filename) 
sub GetFormattedFile {
	my ($dirname, $filename) = @_;
	if($^O eq "MSWin32") {
	    my $path = $dirname . qq(\\) . $filename . ".log";
	}
	else {
	    my $path = $dirname . qq(/) . $filename . ".log";
	}
	my ($line, $text);
	$text = "";
	if(open(FILE, $path)) {
		while($line = <FILE>) {
			if($line =~ m/^#/) {
				$line = qq(<B>) . $line . qq(</B>) . qq(<BR>);
			}
			else {
				$line .= qq(<BR>);
			}		 
			$text .= $line;
		}
		close(FILE);
	}
	else {
		$text = qq(<B>) . "Can't open $path: $!" . qq(</B>) . qq(<BR>);
	}
	return $text;
}

# sub ProcessFile($dirname, $filename, $index);
sub ProcessFile {
	my ($dirname, $filename, $index) = @_;
	my $path = $dirname . qq(/) . $filename . ".log";
	
	sysopen(FILE, $path, 0);
	my $numBytesRead = 0;
	my ($chunk, $isincomplete);
	my $offset = 0;
	my %samples;
	my @params;
	my $count;
	while($numBytesRead = sysread(FILE, $chunk, 10240, $offset)) {
		if(!defined($numBytesRead)) {
			next if $! =~ /^Interrupted/;
			die "system read error: $!\n";
		}
		if($chunk !~ m/^(.*)[\r\n]$/) {
			$isincomplete = 1;
		}
		else {
			$isincomplete = 0;
		}
		my @lines = split /\r|\n/, $chunk;
		$offset = 0;
		if($isincomplete == 1) {
			$chunk = pop @lines;
			$offset = length $chunk;
		}
		while(@lines) {
			$line = shift @lines;
			if(($line !~ m/^#/) && ($line !~ m/^(\s*)$/)){
				@params = split /\s+/, $line;
				$count = $samples{$params[$index]};
				$samples{$params[$index]} = (defined($count))?($count+1):1; 
			}
		}
		undef @lines;
	}
	close(FILE);
	
	if(($isincomplete == 1) && ($chunk !~ m/^#(.*)/)) {
		@params = split /\s+/, $chunk;
		$count = $samples{$params[$index]};
		$samples{$params[$index]} = (defined($count))?($count+1):1;  	
	}
	
	my @dArr;
	my $sampl;
	my $i = 0;
	foreach $sampl (keys %samples) {
		$dArr[$i] = ();
		$dArr[$i]->[0] = $sampl;
		$dArr[$i]->[1] = $samples{$sampl};
		$i++;
	}
	return \@dArr;
}


# sub ConvertTimeToStr($timeInmSec)
sub ConvertTimeToStr {
	my $timeStr;
	my $sec = $_[0]/1000;
	my $days = int ($sec / 86400);
	if($days != 0) { $timeStr = "$days days"; }	
	$sec %= 86400;
	my $hr = int ($sec / 3600);
	if($hr != 0) { $timeStr .= " $hr hrs"; }	
	$sec %= 3600;
	my $min = int ($sec / 60);
	if($min != 0) { $timeStr .= " $min min"; }	
	$sec %= 60;
	if($sec != 0) { $timeStr .= " $sec sec"; }
	return $timeStr; 
}

# sub FormatData(value, condition, trueAction, trueSuffix, falseAction, falseSuffix)
sub FormatData {
	$expr = $_[0] . $_[1];
	$trueResult = $_[0] . $_[2];
	$falseResult = $_[0] . $_[4]; 
	if(eval($expr)) {
		$result = int(eval($trueResult));
		return ("$result" . $_[3]);
	}
	else {
	 	$result = int(eval($falseResult));
		return ("$result" . $_[5]);
	}
}

# sub FormatSelectOption(value, condition, selectName, option1, option2, selectOptionNumberIfTrue) 
sub FormatSelectOption {
	$expr = $_[0] . $_[1];
	$name = $_[2];
	$opt1Name = $_[3];
	$opt2Name = $_[4];
	$selectOpt = $_[5];
	$result = eval($expr); 
	if(($result && ($selectOpt == 1)) || (!$result && ($selectOpt == 2))) {
		return (qq(<SELECT NAME="$_[2]"><OPTION SELECTED>$opt1Name<OPTION>$opt2Name</SELECT>));
	}
	elsif(($result && ($selectOpt == 2)) || (!$result && ($selectOpt == 1))) {
		return (qq(<SELECT NAME="$_[2]"><OPTION>$opt1Name<OPTION SELECTED>$opt2Name</SELECT>));
	}
	else {
	 	return (qq(<SELECT NAME="$_[2]"><OPTION>$opt1Name<OPTION>$opt2Name</SELECT>));
	}
}

# sub FormatRadioButton(name, value, 1|0, checked|unchecked)  
sub FormatRadioButton {
	if((($_[2] == 1) && ($_[3] eq "checked")) || (($_[2] == 0) && ($_[3] eq "unchecked"))) {
		return (qq(<INPUT TYPE=radio NAME="$_[0]" VALUE=$_[1] CHECKED>));
	}
	elsif((($_[2] == 1) && ($_[3] eq "unchecked")) || (($_[2] == 0) && ($_[3] eq "checked"))){
	 	return (qq(<INPUT TYPE=radio NAME="$_[0]" VALUE=$_[1]>));
	}
}

# HtmlEscape
# Convert &, < and > codes in text to HTML entities
sub HtmlEscape
{
	local($tmp);
	$tmp = $_[0];
	$tmp =~ s/&/&amp;/g;
	$tmp =~ s/</&lt;/g;
	$tmp =~ s/>/&gt;/g;
	$tmp =~ s/\"/&#34;/g;
	return $tmp;
}

# sub StartServer(prog)
sub StartServer() {
	my $prog = $_[0];
	if($^O eq "MSWin32") {
		#eval "require Win32::Service";
		#if(!$@) {
		#	Win32::Service::StartService(NULL, "Darwin Streaming Server");
		#}
		#eval "require Win32::Process";
		#if(!$@) {
		#	if(Win32::Process::Create($processObj, $prog, "", 0, DETACHED_PROCESS, ".") != 0) {
		#		$processObj->SetPriorityClass(NORMAL_PRIORITY_CLASS);
		#		$processObj->Wait(0);
		#	}
		#}
	}
	else {
		# fork off a child and exec the server
		if(!($pid = fork())) {
			exec $prog;
			exit;
		}
	}
}

# GetServerStateString(state)
# Maps the given number to the server state
# returns the corresponding string
# qtssStartingUpState|qtssRunningState|qtssRefusingConnectionsState|qtssFatalErrorState|qtssShuttingDownState
sub GetServerStateString {
	my $state = $_[0];
	if(!defined($state)) {
		$state = -1;
	}
	my @serverStateArr = (
		"Starting Up",
		"Running",
		"Refusing Connections",
		"In Fatal Error State",
		"Shutting Down",
		"Idle"
	);
	if($state < 0 || $state > $#serverStateArr) {
		return "Not Running";
	}
	return $serverStateArr[$state];
}

# ParseQueryString (queryString)
# Parses the name=value&name=value ... query string 
# and creates a hash of with names as keys
# Returns the reference to the hash
sub ParseQueryString {
    $qs = shift @_;
    # split it up into an array by the '&' character
    @qs = split(/&/,$qs);
    foreach $i (0 .. $#qs)
    {
	# convert the plus chars to spaces
	$qs[$i] =~ s/\+/ /g;
	
	# convert the hex characters
	$qs[$i] =~ s/%(..)/pack("c",hex($1))/ge;
	
	# split each one into name and value
	($name, $value) = split(/=/,$qs[$i],2);
	
	# create the associative element
	$qs{$name} = $value;
    }
    return \%qs;
}

# ConvertToVersionString ( version )
# converts the number to a hex string 
sub ConvertToVersionString {
    my $version = sprintf "%lx", $_[0];
    
    my @resultArr = unpack "h", $version;
    my $result = ($resultArr[0] == 0)? "0" : "$resultArr[0]";
    $result .= "\.";
    $result .= ($resultArr[1] == 0)? "0" : "$resultArr[1]";
    
    return $result;
}

1; #return true    
