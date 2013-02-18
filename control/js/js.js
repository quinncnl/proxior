var curlist;
var target;


var listshtml = "";
function loadlist() {
    if (listshtml != "") $("#listname").html(listshtml);
    else
	$.ajax({
	    url: "http://"+target+"/getlists",
	    success: function( data ) {
		var list = data.split("\n");
		var first = true;
		for(var i=list.length-2; i>=0; i--) {
		    var name=list[i].split(",")[0];
		    if (first) {
			getlist(name);
			first = false;
		    }
		    listshtml +="<option name='"+name+"'>"+name+"</option>";
		}

		$("#listname").html(listshtml);

	    }
	});
}

$(function() {
    target = $.cookie('target');
    if (target == undefined)
	target = "192.168.0.6:9999";

    loadlist();
    $( "#tabs" ).tabs({
	activate: function( event, ui ) {
	    switch(ui.newTab.index()) {
	    case 0:
		loadlist();
		break;
	    case 1:
		$.ajax({
		    url: "http://"+target+"/getlog",
		    success: function( list ) {
			$( "#log" ).html(list);
		    }
		});
		break;
	    }
	}
    });

    curlist = listname;
    $("#target_proxy").val(target);

});

function douptarget() {
    $.cookie('target',  $("#target_proxy").val(), { expires: 7 });
    target = $("#target_proxy").val();
}

function do_find() {

    $.ajax({
	type: "POST",
	url: "http://"+target+"/updatelist?list="+curlist,
	data: {data: encodeURIComponent($("#data").val())},
	success: function(rs) {
	   
	}
    });
}

function dormlog() {

    $.ajax({
	type: "POST",
	url: "http://"+target+"/rmlog",
	success: function(rs) {
	    $( "#log" ).html("");
	}
    });
}
