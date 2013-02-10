var curlist;

function getlist(listname) {
    curlist = listname;
    $.ajax({
	url: "http://192.168.0.6:9999/getlist?"+listname,
	success: function( list ) {
	    $( "#data" ).html(list);
	}
    });
}

function loadlist() {
    $.ajax({
	url: "http://192.168.0.6:9999/getlists",
	success: function( data ) {
	    var list = data.split("\n");
	    var first = true;
	    for(var i=list.length-2; i>=0; i--) {
		var name=list[i].split(",")[0];
		if (first) {
		    getlist(name);
		    first = false;
		}
		$("#listname").append("<option name='"+name+"'>"+name+"</option>");
	    }
	}
    });
}

$(function() {
    
    loadlist();
    $( "#tabs" ).tabs({
	activate: function( event, ui ) {
	    switch(ui.newTab.index()) {
	    case 0:
		loadlist();
		break;
	    case 1:
		$.ajax({
		    url: "http://192.168.0.6:9999/getlog",
		    success: function( list ) {
			$( "#log" ).html(list);
		    }
		});
		break;
	    }
	}
    });

    curlist = listname;

});

function dosubmit() {

    $.ajax({
	type: "POST",
	url: "http://192.168.0.6:9999/updatelist?list="+curlist,
	data: {data: encodeURIComponent($("#data").val())},
	success: function(rs) {
	    alert(rs);
	}
    });
}

function dormlog() {

    $.ajax({
	type: "POST",
	url: "http://192.168.0.6:9999/rmlog",
	success: function(rs) {
	    alert(rs);
	}
    });
}
