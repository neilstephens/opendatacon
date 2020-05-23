/* this function will convert json data to treeview data so that we can easily show on treeview */
function convert_to_treeview_data(json) {
    var data = [];
    var count = 0;

    for (var key of Object.keys(json)) {
        if (key.length > 0) {
            if (typeof json[key] == 'object') {
                const nodes = get_node(json[key]);
                data.push(build_node(key, null, count, nodes));
            } else {
                data.push(build_node(key, json[key], count, null));
            }
        }
        ++count;
    }

    return data;
}

/* recursive function to build the treeview nodes, depending on object type[array or dictionary] or value pair */
function get_node(data) {
    if (data) {
        var node = [];
        var count = 0;

        if (Array.isArray(data) == true) {
            for (var i = 0; i < data.length; ++i) {
                var count = 0;
                for (var key of Object.keys(data[i])) {
                    if (typeof data[i][key] == 'object') {
                        var sub_node = get_node(data[i][key]);
                        node.push(build_node(key, null, count, sub_node));
                    } else {
                        node.push(build_node(key, data[i][key], count, null));
                    }

                    ++count;
                }
            }
        } else {
            for (var key of Object.keys(data)) {
                if (typeof data[key] == 'object') {
                    var sub_key = get_node(data[key]);
                    node.push(build_node(key, null, count, sub_key));
                } else {
                    node.push(build_node(key, data[key], count, null));
                }

                ++count;
            }
        }

        return node;
    }
}

/* this function build the node or sub node depending on the value or nodes are present or not */
function build_node(key, value, count, nodes) {
    var node = {};
    var d = key;
    if (value != null) {
        d += " : " + value;
    }
    node["text"] = d;
    node["tags"] = "'" + count + "'";
    node["href"] = "#" + "_" + d;
    if (nodes != null)
        node["nodes"] = nodes;

    return node;
}
