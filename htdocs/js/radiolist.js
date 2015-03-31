/* ympd
   (c) 2013-2014 Andrew Karpow <andy@ndyk.de>
   This project's homepage is: http://www.ympd.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

var socket;
var last_state;
var current_app;
var pagination = 0;
var queue_pagination = 0;
var radio_pagination = 0;
var browsepath;
var lastSongTitle = "";
var current_song = new Object();
var MAX_ELEMENTS_PER_PAGE = 5;
var current_song_pos = 0;
var next_song_pos = 0;

var app = $.sammy(function() {
    this.get("/", function(context) {
        context.redirect("#/0");
    });
});

$(document).ready(function(){
    webSocketConnect();
});


function webSocketConnect() {
    if (typeof MozWebSocket != "undefined") {
        socket = new MozWebSocket(get_appropriate_ws_url());
    } else {
        socket = new WebSocket(get_appropriate_ws_url());
    }

    try {
        socket.onopen = function() {
            console.log("connected");
            /*$('.top-right').notify({
                message:{text:"Connected to ympd"},
                fadeOut: { enabled: true, delay: 500 }
            }).show();
*/
            socket.send("MPD_API_GET_RADIO," + 0);
            app.run();
        }

        socket.onmessage = function got_packet(msg) {
            if(msg.data === last_state || msg.data.length == 0)
                return;

            var obj = JSON.parse(msg.data);
            switch (obj.type) {
                case "radio":
                    $('#radiolist').empty();
                    for(var ii in obj.data) {
                        var namestring;
                        var stringlength = obj.data[ii].name.length;
                        if (stringlength > 14) {
                            namestring = obj.data[ii].name.substr(0, 11)+"-"
                                +obj.data[ii].name.substr(stringlength-2, stringlength);
                        } else
                            namestring = obj.data[ii].name;
                        console.log(namestring);
                        $('#radiolist')
                            .append("<li title=\""+obj.data[ii].name+"\">"+namestring+"</li>");
                    }
                    $('#radiolist > li').on({
                        click: function() {
                            console.log($(this).attr("title"));
                            socket.send("MPD_API_SET_RADIO," + $(this).attr("title"));
                        },
                    });
/*                    $('#crabsalad > tbody').empty();
                    for(var ii in obj.data) {
                        console.log(obj.data[ii].name);
                        $('#crabsalad > tbody').append(
                                "<tr dest=\"" + obj.data[ii].name + "\">" +
                                "<td><H4><span class=\"label label-success\">"+ obj.data[ii].name +"</span></H4></td>" +
                                "</tr>");
                    }
                    $('#crabsalad > tbody > tr').on({
                        click: function() {
                            $('.modal').modal('hide');
                            socket.send("MPD_API_SET_RADIO," + $(this).attr("dest"));
                        },
                    });
*/
                    break;
                case "state":
                    updatePlayIcon(obj.data.state);
                    $('#volume').text(obj.data.volume);
                    updateVolumeIcon(obj.data.volume);
                    updateRadioIcon(obj.data.radio_status);
                    updateRandomIcon(obj.data.random);

                    if(JSON.stringify(obj) === JSON.stringify(last_state))
                        break;

                    current_song.totalTime  = obj.data.totalTime;
                    current_song.currentSongId = obj.data.currentsongid;
                    current_song_pos = obj.data.songpos;
                    $('#songrating').text(obj.data.songrating);
                    $('#currentpos').text(current_song_pos+"("+obj.data.queue_len+")");
                    next_song_pos = obj.data.nextsongpos;
                    $('#nextpos').text(next_song_pos);
                    var total_minutes = Math.floor(obj.data.totalTime / 60);
                    var total_seconds = obj.data.totalTime - total_minutes * 60;

                    var elapsed_minutes = Math.floor(obj.data.elapsedTime / 60);
                    var elapsed_seconds = obj.data.elapsedTime - elapsed_minutes * 60;

                    /*$('#volumeslider').slider(obj.data.volume);
                    var progress = Math.floor(100*obj.data.elapsedTime/obj.data.totalTime);
                    $('#progressbar').slider(progress);*/


                    $('#counter')
                    .text(elapsed_minutes + ":" +
                        (elapsed_seconds < 10 ? '0' : '') + elapsed_seconds + "/" +
                        total_minutes + ":" + (total_seconds < 10 ? '0' : '') + total_seconds);

                    $('#salamisandwich > tbody > tr').removeClass('active').css("font-weight", "");
                    $('#salamisandwich > tbody > tr[trackid='+obj.data.currentsongid+']').addClass('active').css("font-weight", "bold");

                    if(obj.data.random)
                        $('#btnrandom').addClass("active")
                    else
                        $('#btnrandom').removeClass("active");

                    if(obj.data.consume)
                        $('#btnconsume').addClass("active")
                    else
                        $('#btnconsume').removeClass("active");

                    if(obj.data.single)
                        $('#btnsingle').addClass("active")
                    else
                        $('#btnsingle').removeClass("active");

                    if(obj.data.repeat)
                        $('#btnrepeat').addClass("active")
                    else
                        $('#btnrepeat').removeClass("active");

                    last_state = obj;
                    break;
                case "disconnected":
                    if($('.top-right').has('div').length == 0)
                        $('.top-right').notify({
                            message:{text:"ympd lost connection to MPD "},
                            type: "danger",
                            fadeOut: { enabled: true, delay: 1000 },
                        }).show();
                    break;
                case "current_radio":
                    //console.log(obj.data.name+" "+obj.data.logo);
                    $('#currentradio').text(obj.data.name);
                    $('#currentradiostatus').text(obj.data.status);
                    $('#currentradiosize').text(obj.data.size);
                    if (obj.data.logo) {
                        var logoimage = document.getElementById("radioimage");
                        radioimage.src = obj.data.logo;
                    }
                    break;
                case "mpdhost":
                    $('#mpdhost').val(obj.data.host);
                    $('#mpdport').val(obj.data.port);
                    if(obj.data.passwort_set)
                        $('#mpd_password_set').removeClass('hide');
                    break;
                case "error":
                    $('.top-right').notify({
                        message:{text: obj.data},
                        type: "danger",
                    }).show();
                default:
                    break;
            }


        }
        socket.onclose = function(){
            console.log("disconnected");
            $('.top-right').notify({
                message:{text:"Connection to ympd lost, retrying in 3 seconds "},
                type: "danger",
                onClose: function () {
                    webSocketConnect();
                }
            }).show();
        }

    } catch(exception) {
        alert('<p>Error' + exception);
    }

}

function get_appropriate_ws_url()
{
    var pcol;
    var u = document.URL;

    /*
    /* We open the websocket encrypted if this page came on an
    /* https:// url itself, otherwise unencrypted
    /*/

    if (u.substring(0, 5) == "https") {
        pcol = "wss://";
        u = u.substr(8);
    } else {
        pcol = "ws://";
        if (u.substring(0, 4) == "http")
            u = u.substr(7);
    }

    u = u.split('/');

    return pcol + u[0];
}

var download_artist_info = function(artist)
{
    $.get("http://ws.audioscrobbler.com/2.0/?method=artist.getinfo&artist=" +
        artist +
        "&autocorrect=1&api_key=ecb4076a85c81aae38a7e8f11e42a0b1&format=json&callback=",
    function(lastfm)
    {
        var art_url;
        var artimage = document.getElementById("artimage");
        if (lastfm && lastfm.artist) {
            if (lastfm.artist.image[2]['#text']) {
                art_url = lastfm.artist.image[1]['#text'];
                console.log('art_url :'+art_url);
                artimage.src = art_url;
                socket.send('MPD_API_DB_ARTIST,'+artist+'|'+art_url);
            } else {
                console.log("there is no artist image");
                //artimage.src = "/images/art.png";
            }
        } else {
            console.log("there is no artist info");
            //artimage.src = "/images/art.png";
        }
    });
}

var updateVolumeIcon = function(volume)
{
    $("#volume-icon").removeClass("glyphicon-volume-off");
    $("#volume-icon").removeClass("glyphicon-volume-up");
    $("#volume-icon").removeClass("glyphicon-volume-down");

    if(volume == 0) {
        $("#volume-icon").addClass("glyphicon-volume-off");
    } else if (volume < 50) {
        $("#volume-icon").addClass("glyphicon-volume-down");
    } else {
        $("#volume-icon").addClass("glyphicon-volume-up");
    }
}

var updatePlayIcon = function(state)
{
    $("#play-icon").removeClass("glyphicon-play")
    .removeClass("glyphicon-pause");
    $('#track-icon').removeClass("glyphicon-play")
    .removeClass("glyphicon-pause")
    .removeClass("glyphicon-stop");

    if(state == 1) { // stop
        $("#play-icon").addClass("glyphicon-play");
        $('#track-icon').addClass("glyphicon-stop");
    } else if(state == 2) { // pause
        $("#play-icon").addClass("glyphicon-pause");
        $('#track-icon').addClass("glyphicon-play");
    } else { // play
        $("#play-icon").addClass("glyphicon-play");
        $('#track-icon').addClass("glyphicon-pause");
    }
}

var updateRadioIcon = function(radio_status)
{
    $("#radio-icon").removeClass("glyphicon-cloud-download")
    .removeClass("glyphicon-hdd");
    if (radio_status == 1) { //on
        $("#radio-icon").addClass("glyphicon-cloud-download");
    } else
    {
        $("#radio-icon").addClass("glyphicon-hdd");
    }
}

var updateRandomIcon = function(random)
{
    $("#random-icon").removeClass('gray');
    if (random == 0) {
        $("#random-icon").addClass('gray');
    } else {
        $("#random-icon").removeClass('gray');
    }
}

function updateDB() {
    socket.send('MPD_API_UPDATE_DB');
    $('.top-right').notify({
        message:{text:"Updating MPD Database... "}
    }).show();
}

function listArtists() {
    socket.send('MPD_API_LIST_ARTISTS');
}

function clickPlay() {
    if($('#track-icon').hasClass('glyphicon-stop'))
        socket.send('MPD_API_SET_PLAY');
    else
        socket.send('MPD_API_SET_PAUSE');
}

function clickRandom() {
    socket.send("MPD_API_TOGGLE_RANDOM," + ($("#random-icon").hasClass('gray') ? 1 : 0));
}

function basename(path) {
    return path.split('/').reverse()[0];
}

$('#btnradio').on('click', function (e) {
    socket.send("RADIO_TOGGLE_RADIO," + ($(this).hasClass('active') ? 0 : 1));

});
$('#btnrandom').on('click', function (e) {
    socket.send("MPD_API_TOGGLE_RANDOM," + ($(this).hasClass('active') ? 0 : 1));

});
$('#btnconsume').on('click', function (e) {
    socket.send("MPD_API_TOGGLE_CONSUME," + ($(this).hasClass('active') ? 0 : 1));

});
$('#btnsingle').on('click', function (e) {
    socket.send("MPD_API_TOGGLE_SINGLE," + ($(this).hasClass('active') ? 0 : 1));
});
$('#btnrepeat').on('click', function (e) {
    socket.send("MPD_API_TOGGLE_REPEAT," + ($(this).hasClass('active') ? 0 : 1));
});

$('#btnnotify').on('click', function (e) {
    if($.cookie("notification") === "true") {
        $.cookie("notification", false);
    } else {
        Notification.requestPermission(function (permission) {
            if(!('permission' in Notification)) {
                Notification.permission = permission;
            }

            if (permission === "granted") {
                $.cookie("notification", true, { expires: 424242 });
                $('btnnotify').addClass("active");
            }
        });
    }
});

function getRadio() {
    console.log('get list of radio stations');
    socket.send('MPD_API_GET_RADIO,'+radio_pagination);
}

function getQueue() {
    console.log('get queue');
    app.setLocation('#/'+queue_pagination);
}
function getBrowse() {
    console.log('get browse mpd database');
    app.setLocation('#/browse/'+pagination+'/'+browsepath);
}

function getHost() {
    socket.send('MPD_API_GET_MPDHOST');

    function onEnter(event) {
      if ( event.which == 13 ) {
        confirmSettings();
      }
    }

    $('#mpdhost').keypress(onEnter);
    $('#mpdport').keypress(onEnter);
    $('#mpd_pw').keypress(onEnter);
    $('#mpd_pw_con').keypress(onEnter);
}

$('#search').submit(function () {
    app.setLocation("#/search/"+$('#search > div > input').val());
    $('#wait').modal('show');
    setTimeout(function() {
        $('#wait').modal('hide');
    }, 10000);
    return false;
});

$('.page-btn').on('click', function (e) {
    /*switch ($(this).text()) {
        case "Next":
            pagination += MAX_ELEMENTS_PER_PAGE;
            break;
        case "Prev":
            pagination -= MAX_ELEMENTS_PER_PAGE;
            if(pagination <= 0)
                pagination = 0;
            break;
    }*/

    switch(current_app) {
        case "queue":
            switch ($(this).text()) {
                case "Next":
                    queue_pagination += MAX_ELEMENTS_PER_PAGE;
                    break;
                case "Prev":
                    queue_pagination -= MAX_ELEMENTS_PER_PAGE;
                    if(queue_pagination <= 0)
                        queue_pagination = 0;
                    break;
            }
            app.setLocation('#/'+queue_pagination);
            break;
        case "browse":
            switch ($(this).text()) {
                case "Next":
                    pagination += MAX_ELEMENTS_PER_PAGE;
                    break;
                case "Prev":
                    pagination -= MAX_ELEMENTS_PER_PAGE;
                    if(pagination <= 0)
                        pagination = 0;
                    break;
            }
            app.setLocation('#/browse/'+pagination+'/'+browsepath);
            break;
    }
    e.preventDefault();
});

function confirmSettings() {
    if($('#mpd_pw').val().length + $('#mpd_pw_con').val().length > 0) {
        if ($('#mpd_pw').val() !== $('#mpd_pw_con').val())
        {
            $('#mpd_pw_con').popover('show');
            setTimeout(function() {
                $('#mpd_pw_con').popover('hide');
            }, 2000);
            return;
        } else
            socket.send('MPD_API_SET_MPDPASS,'+$('#mpd_pw').val());
    }
    socket.send('MPD_API_SET_MPDHOST,'+$('#mpdport').val()+','+$('#mpdhost').val());
    $('#settings').modal('hide');
}

$('#mpd_password_set > button').on('click', function (e) {
    socket.send('MPD_API_SET_MPDPASS,');
    $('#mpd_pw').val("");
    $('#mpd_pw_con').val("");
    $('#mpd_password_set').addClass('hide');
})

function notificationsSupported() {
    return "Notification" in window;
}

function songNotify(title, artist, album) {
    /*var opt = {
        type: "list",
        title: title,
        message: title,
        items: []
    }
    if(artist.length > 0)
        opt.items.push({title: "Artist", message: artist});
    if(album.length > 0)
        opt.items.push({title: "Album", message: album});
*/
    //chrome.notifications.create(id, options, creationCallback);

    var textNotification = "";
    if(typeof artist != 'undefined' && artist.length > 0)
        textNotification += " " + artist;
    if(typeof album != 'undefined' && album.length > 0)
        textNotification += "\n " + album;

	var notification = new Notification(title, {icon: 'assets/favicon.ico', body: textNotification});
    setTimeout(function(notification) {
        notification.close();
    }, 3000, notification);
}
