function check(arr, key) {
	var out = [];
	for (var a in arr) {
		if (arr[a] == undefined)
		{
			out[a] = null;
		}
		else if (key in arr[a])
		{
			out[a] = arr[a][key];
		}
		else
		{
			out[a] = null;
		}
	}
	return out;
}

function merge(left, right) {
	var result = [];
	while(left.length || right.length) {
	  if(left.length && right.length) {
		if(left[0] < right[0]) {
		  result.push(left.shift());
		} else if(left[0] > right[0]) {
		  result.push(right.shift());
		} else {
		  result.push(left.shift());
		  right.shift();
			//throw away dupe
		}
	  } else if (left.length) {
		result.push(left.shift());
	  } else {
		result.push(right.shift());
	  }
	}
	return result;
  }

function outputJsonTree(parent,s,o) {
		
	var ol = document.createElement('ol');
	ol.setAttribute('class','JsonArray');
	parent.appendChild(ol);
	
	var keys = [];
	for (var i in o) {
		if (o[i] != null)
		{
			keys = merge(keys,Object.keys(o[i]));
			//keys = keys.concat(Object.keys(o[i]));
		}
	}
	
	for (var i in keys) {
		k = keys[i];
		
		isObject = false;
		for (var j in o) {
			if (o[j] != null && o[j][k] != null && typeof(o[j][k])=="object")
			{
				isObject = true;
			}
		}
				
		//if (b[k] !== null && typeof(b[k])=="object") {
		if (isObject) {
			var li = document.createElement('li');
			ol.appendChild(li);

			var span = document.createElement('span');
			span.setAttribute('class','JsonKey');
			span.textContent = s + "[" + k + "]";
			li.appendChild(span);
								
			//going on step down in the object tree!!
			outputJsonTree(li, s + "[" + k + "]", check(o,k));
		} else
		{
			var els = check(o,k);
			var li = document.createElement('li');
			ol.appendChild(li);
		
			var span = document.createElement('span');
			span.setAttribute('class','JsonKey');
			span.textContent = s + "[" + k + "]";
			li.appendChild(span);

			for (var a in els)
			{
				var span = document.createElement('span');
				span.setAttribute('class','JsonValue');
				span.textContent = els[a];
				li.appendChild(span);
			}			
		}
	}
}