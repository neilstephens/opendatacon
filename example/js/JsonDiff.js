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

function AddJsonTree(tbody,s,o) {		
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
		var name = s + ':' + k ;

		isObject = false;
		for (var j in o) {
			if (o[j] != null && o[j][k] != null && typeof(o[j][k])=="object")
			{
				isObject = true;
			}
		}
				
		if (isObject) {
			var tr = document.createElement('tr');
			tr.setAttribute('data-tt-id', name);
			tr.setAttribute('data-tt-parent-id', s);
			tbody.appendChild(tr);

			var td = document.createElement('td');
			td.setAttribute('class','JsonKey');
			td.textContent = k;
			tr.appendChild(td);
			
			for (var a in o)
			{
				var td = document.createElement('td');
				tr.appendChild(td);
			}
			
			//going on step down in the object tree!!
			AddJsonTree(tbody, name, check(o,k));
		} else
		{
			var els = check(o,k);
			var tr = document.createElement('tr');
			tr.setAttribute('data-tt-id', name);
			tr.setAttribute('data-tt-parent-id', s);
			tbody.appendChild(tr);
		
			var td = document.createElement('td');
			td.setAttribute('class','JsonKey');
			td.textContent = k;
			tr.appendChild(td);

			for (var a in els)
			{
				var td = document.createElement('td');
				td.setAttribute('class','JsonValue');
				td.textContent = els[a];
				tr.appendChild(td);
			}			
		}
	}
}

function BuildJsonTree(table,s,o) {
	var thead = document.createElement('thead');
	table.appendChild(thead);
	var th = document.createElement('th');
	th.textContent = 'Variable';
	thead.appendChild(th);
	
	for (var a in o) {
		var th = document.createElement('th');
		th.textContent = a;
		thead.appendChild(th);			
	}
	
	var tbody = document.createElement('tbody');
	table.appendChild(tbody);
	AddJsonTree(tbody,s,o);
}