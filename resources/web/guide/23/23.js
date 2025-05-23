
var m_ProfileItem;

var FilamentPriority=new Array( "pla","abs","pet","tpu","pc");
var VendorPriority=new Array("bambu lab","bambulab","bbl","kexcelled","polymaker","esun","generic");

var mPrintName;
var mModelName;
var mModelNozzle;

function OnInit()
{
	TranslatePage();
    OnSelectMenu(1);
	
	RequestProfile();
	
	RequestCustomFilaments();
	//TestCustomFilaments();
	//OnSelectMenu(2);
}

function RequestProfile()
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="request_userguide_profile";
	
	SendWXMessage( JSON.stringify(tSend) );
}

//function RequestModelSelect()
//{
//	var tSend={};
//	tSend['sequence_id']=Math.round(new Date() / 1000);
//	tSend['command']="request_userguide_modelselected";
//	
//	SendWXMessage( JSON.stringify(tSend) );
//}

function areFloatsEqual(float1, float2, epsilon = 0.001)
{
    return Math.abs(float1 - float2) < epsilon;
}

function handleStudioCmd(pVal)
{
	let strCmd=pVal['command'];
	//alert(strCmd);

	if(strCmd=='response_userguide_profile')
	{
		//角色
		if (pVal.role_type === '0') 
		{
			document.getElementById("Title").style.display = "none";
		}

		if (pVal.color_mode === '0') {
			setTimeout(() => {
				changeTheme()				
			}, 50);
		}

		m_ProfileItem=pVal['response'];
		mPrintName=pVal['printName'];
		mModelName=pVal['modelName'];
		mModelNozzle=parseFloat(pVal['nozzleSelect']);

		SortUI();

		MachineClick();
	}
	else if(strCmd=='update_custom_filaments')
	{
		UpdateCustomFilaments( pVal['data'] );
	}
}

function GetFilamentShortname( sName )
{
	let sShort=sName.split('@')[0].trim();
	
	return sShort;
}


function SortUI()
{
	var ModelList=new Array();
	
	let nMode=m_ProfileItem["model"].length;
	for(let n=0;n<nMode;n++)
	{
		let OneMode=m_ProfileItem["model"][n];
		
		if( OneMode["nozzle_selected"]!="" )
			ModelList.push(OneMode);
	}
	
	//machine
//	let HtmlMachine='';
//	
//	let nMachine=m_ProfileItem['machine'].length;
//	for(let n=0;n<nMachine;n++)
//	{
//		let OneMachine=m_ProfileItem['machine'][n];
//		
//		let sName=OneMachine['name'];
//		let sModel=OneMachine['model'];
//	
//		if( ModelList.in_array(sModel) )
//		{
//			HtmlMachine+='<div><input type="checkbox" mode="'+sModel+'" onChange="MachineClick()" />'+sName+'</div>';
//		}
//	}
//	
//	$('#MachineList .CValues').append(HtmlMachine);	
//	$('#MachineList .CValues input').prop("checked",true);
//	if(nMachine<=1)
//	{
//		$('#MachineList').hide();
//	}
	
	//model
	let HtmlMode='';
	nMode=ModelList.length;
	for(let n=0;n<nMode;n++)
	{
		let sModel=ModelList[n];	

		HtmlMode+='<div ><input type="checkbox" mode="'+sModel['model']+'"  nozzle="'+sModel['nozzle_selected']+'"   onChange="MachineClick()" />'+sModel['model']+'</div>';
	}
	
	$('#MachineList .CValues').append(HtmlMode);	
	$('#MachineList .CValues input').prop("checked",true);
	$('#MachineList').hide();
	// if(nMode<=1)
	// {
	// 	$('#MachineList').hide();
	// }

	$('#MachineId .Value').text(mPrintName);

	//Filament
	let HtmlFilament='';
	let SelectNumber=0;

	var TypeHtmlArray={};
    var VendorHtmlArray={};
	for( let key in m_ProfileItem['filament'] )
	{
		let OneFila=m_ProfileItem['filament'][key];
		
		//alert(JSON.stringify(OneFila));
		
		let fWholeName=OneFila['name'].trim();
		let fShortName=GetFilamentShortname( OneFila['name'] );
		let fVendor=OneFila['vendor'];
		let fType=OneFila['type'];
		let fSelect=OneFila['selected'];
		let fModel=OneFila['models']
		
//		if(OneFila['name'].indexOf("Bambu PA-CF")>=0)
//		{
//			alert( fShortName+' - '+fVendor+' - '+fType+' - '+fSelect+' - '+fModel )
//			
//			let b=1+2;
//		}
		
        let bFind=false;		
		//let bCheck=$("#MachineList input:first").prop("checked");
		if( fModel=='')
		{
			bFind=true;
		}
		else
		{
			//check in modellist		    
		    let nModelAll=ModelList.length;
		    for(let m=0;m<nModelAll;m++)
		    {
	    		let sOne=ModelList[m];
			
				let OneName=sOne['model'];
				let NozzleArray=sOne["nozzle_selected"].split(';');
				
				let nNozzle=NozzleArray.length;
				
				for( let b=0;b<nNozzle;b++ )
				{
					let nowModel= OneName+"++"+NozzleArray[b];
					if(fModel.indexOf(nowModel)>=0)
					{
						bFind=true;
						break;
					}
				}
			}
		}
		
		if(bFind)
		{
			//Type
			let LowType=fType.toLowerCase();
		    if(!TypeHtmlArray.hasOwnProperty(LowType))
		    {
			    let HtmlType='<div class="checkItem"><input type="checkbox" filatype="'+fType+'" onChange="FilaClick()"   />'+fType+'</div>';
			
				TypeHtmlArray[LowType]=HtmlType;
		    }
			
			//Vendor
			let lowVendor=fVendor.toLowerCase();
			if(!VendorHtmlArray.hasOwnProperty(lowVendor))
		    {
			    let HtmlVendor='<div class="checkItem"><input type="checkbox" vendor="'+fVendor+'"  onChange="VendorClick()" />'+fVendor+'</div>';
				
				VendorHtmlArray[lowVendor]=HtmlVendor;
		    }
			
			//Filament
			let pFila=$("#ItemBlockArea input[vendor='"+fVendor+"'][filatype='"+fType+"'][name='"+fShortName+"']");
			if ($("#ItemBlockArea [filatype='" + fType + "']").length === 0) {
				$("#ItemBlockArea").append(`<div class="filaTypeItem" filatype="${fType}"><div class="filaType">${fType}</div><div class="filaMitem"></div></div>`);
			}
	        if(pFila.length==0)
		    {
			    let HtmlFila='<div class="MItem"><input type="checkbox" vendor="'+fVendor+'"  filatype="'+fType+'" filalist="'+fWholeName+';'+'"  model="'+fModel+'" name="'+fShortName+'" />'+fShortName+'</div>';
			
			    $("#ItemBlockArea [filatype='" + fType + "'] .filaMitem").append(HtmlFila);
		    } 
			else
			{
				let strModel=pFila.attr("model");
				let strFilalist=pFila.attr("filalist");
				
				pFila.attr("model", strModel+fModel);
				pFila.attr("filalist", strFilalist+fWholeName+';');
			}
			
		    if(fSelect*1==1)
			{
				$("#ItemBlockArea input[vendor='"+fVendor+"'][filatype='"+fType+"'][name='"+fShortName+"']").prop("checked",true);
				SelectNumber++;
			}
//			else
//				$("#ItemBlockArea input[vendor='"+fVendor+"'][model='"+fModel+"'][filatype='"+fType+"'][name='"+key+"']").prop("checked",false);			
		}
	} 

	//Sort TypeArray
	let TypeAdvNum=FilamentPriority.length;
	for( let n=0;n<TypeAdvNum;n++ )
	{
		let strType=FilamentPriority[n];
		
		if( TypeHtmlArray.hasOwnProperty( strType ) )
		{
			$("#FilatypeList .CValues").append( TypeHtmlArray[strType] );
			delete( TypeHtmlArray[strType] );
		}
	}
    for(let key in TypeHtmlArray )
	{
		$("#FilatypeList .CValues").append( TypeHtmlArray[key] );
	}
	$("#FilatypeList .CValues input").prop("checked",true);
	
	//Sort VendorArray
	let VendorAdvNum=VendorPriority.length;
	for( let n=0;n<VendorAdvNum;n++ )
	{
		let strVendor=VendorPriority[n];
		
		if( VendorHtmlArray.hasOwnProperty( strVendor ) )
		{
			$("#VendorList .CValues").append( VendorHtmlArray[strVendor] );
			delete( VendorHtmlArray[strVendor] );
		}
	}
    for(let key in VendorHtmlArray )
	{
		$("#VendorList .CValues").append( VendorHtmlArray[key] );
	}	
	$("#VendorList .CValues input").prop("checked",true);
	
	//------
	if(SelectNumber==0)
		ChooseDefaultFilament();
}


function ChooseAllMachine()
{
	let bCheck=$("#MachineList input:first").prop("checked");
	
	$("#MachineList input").prop("checked",bCheck);
	
	SortFilament();
}

function MachineClick()
{
	// let nChecked=$("#MachineList input:gt(0):checked").length
	// let nAll    =$("#MachineList input:gt(0)").length
	
	// if(nAll==nChecked)
	// {
	// 	$("#MachineList input:first").prop("checked",true);
	// }
	// else
	// {
	// 	$("#MachineList input:first").prop("checked",false);
	// }
	
	SortFilament();
}

function ChooseAllFilament()
{
	let bCheck=$("#FilatypeList input:first").prop("checked");	
	$("#FilatypeList input").prop("checked",bCheck);	
	
	SortFilament();
}

function FilaClick()
{
	let nChecked=$("#FilatypeList input:gt(0):checked").length
	let nAll    =$("#FilatypeList input:gt(0)").length
	
	if(nAll==nChecked)
	{
		$("#FilatypeList input:first").prop("checked",true);
	}
	else
	{
		$("#FilatypeList input:first").prop("checked",false);
	}
	
	SortFilament();	
}

function ChooseAllVendor()
{
	let bCheck=$("#VendorList input:first").prop("checked");	
	$("#VendorList input").prop("checked",bCheck);	
	
	SortFilament();
}

function VendorClick()
{
	let nChecked=$("#VendorList input:gt(0):checked").length
	let nAll    =$("#VendorList input:gt(0)").length
	
	if(nAll==nChecked)
	{
		$("#VendorList input:first").prop("checked",true);
	}
	else
	{
		$("#VendorList input:first").prop("checked",false);
	}
	
	SortFilament();
}



function SortFilament()
{
	let FilaNodes=$("#ItemBlockArea .MItem");
	let nFilament=FilaNodes.length;
	//$("#ItemBlockArea .MItem").hide();
	
	//更改2：根据传入的打印机以及喷嘴
	//ModelList
	// let pModel=$("#MachineList input:checked");
	// let nModel=pModel.length;
	// let ModelList=new Array();
	// for(let n=0;n<nModel;n++)
	// {
	// 	let OneModel=pModel[n];
		
	// 	let mName=OneModel.getAttribute("mode");
	// 	if( mName=='all' )
	// 	{
	// 		continue;
	// 	}
	// 	else
	// 	{
	// 		let mNozzle = OneModel.getAttribute("nozzle");
	// 		let NozzleArray = mNozzle.split(';');

	// 		for (let bb = 0; bb < NozzleArray.length; bb++) {
	// 			let NewModel = '[' + mName + '++' + NozzleArray[bb] + ']';

	// 			ModelList.push(NewModel);
	// 		}
	// 	}
	// }
	

	let ModelList=new Array();
	let pModel=$("#MachineList input[type='checkbox']");
	let nModel=pModel.length;
	for(let n=0;n<nModel;n++)
	{
		let OneModel=pModel[n];
		let mName = OneModel.getAttribute("mode");
		if (mName == 'all') 
		{
			continue;
		}
		else 
		{
			if (mName == mModelName) 
			{
				let mNozzle = OneModel.getAttribute("nozzle");
				let NozzleArray = mNozzle.split(';');

				for (let bb = 0; bb < NozzleArray.length; bb++) 
				{
					if (areFloatsEqual(NozzleArray[bb], mModelNozzle)) 
					{
						let NewModel = '[' + mName + '++' + NozzleArray[bb] + ']';
						ModelList.push(NewModel);
					}
				}
			}
		}
	}
	
	//TypeList
	let pType=$("#FilatypeList input:gt(0):checked");
	let nType=pType.length;
	let TypeList=new Array();
	for(let n=0;n<nType;n++)
	{
		let OneType=pType[n];
		TypeList.push(  OneType.getAttribute("filatype") );
	}	
	
	//VendorList
	let pVendor=$("#VendorList input:gt(0):checked");
	let nVendor=pVendor.length;
	let VendorList=new Array();
	for(let n=0;n<nVendor;n++)
	{
		let OneVendor=pVendor[n];
		VendorList.push(  OneVendor.getAttribute("vendor") );
	}		
	
	
	//Update Filament UI
	for(let m=0;m<nFilament;m++)
	{
		let OneNode=FilaNodes[m];
		let OneFF=OneNode.getElementsByTagName("input")[0];
		
	    let fModel=OneFF.getAttribute("model");
		let fVendor=OneFF.getAttribute("vendor");
		let fType=OneFF.getAttribute("filatype");
		let fName=OneFF.getAttribute("name");
		
		if(TypeList.in_array(fType) && VendorList.in_array(fVendor))
		{
			let HasModel=false;
			for(let m=0;m<ModelList.length;m++)
			{
				let ModelSrc=ModelList[m];
				
				if( fModel.indexOf(ModelSrc)>=0)
				{
					HasModel=true;
					break;
				}
			}
			
			if(HasModel || fModel==''){
				$(OneNode).parent().parent().show()
			    $(OneNode).show();
			}
			else{
				$(OneNode).hide();
			}
		}
		else
			$(OneNode).hide();
	}

	$("#ItemBlockArea > div").find(".filaMitem").each(function() {
		var displayBlockCount = $(this).children(':visible').length;
		if (displayBlockCount === 0) 
			$(this).parent().hide(); 
		
	});
	
	let filaTitle = []
	$("#VendorList .CValues input:checked").each(function() {
		var contentText = $(this).parent().text().trim();
		filaTitle.push(contentText)
	});

	$("#FilamentList .title").text(filaTitle.join('/')+"：")

}

function ChooseDefaultFilament()
{
	//ModelList
	let pModel=$("#MachineList input:gt(0):checked");
	let nModel=pModel.length;
	let ModelList=new Array();
	for(let n=0;n<nModel;n++)
	{
		let OneModel=pModel[n];
		ModelList.push(  OneModel.getAttribute("mode") );
	}	
	
	//Filament
	let FilaNodes=$("#ItemBlockArea .MItem");
    let nFilament=FilaNodes.length;
    for(let m=0;m<nFilament;m++)
	{
		let OneNode=FilaNodes[m];
		let OneFF=OneNode.getElementsByTagName("input")[0];
		$(OneFF).prop("checked",false);
		
	    let fModel=OneFF.getAttribute("model");
		
		let HasModel=false;
		for(let m=0;m<nModel;m++)
		{
			let ModelSrc=ModelList[m];
		
			if( fModel.indexOf(ModelSrc)>=0)
			{
				HasModel=true;
				break;
			}
		}
			
		if(HasModel)
		    $(OneFF).prop("checked",true);
	}
	
	ShowNotice(0);
}

function SelectAllFilament( nShow )
{
	let bCheck=$("#FilamentList input:first").prop("checked");	
	$('#ItemBlockArea input').prop("checked",bCheck);
	// if( nShow==0 )
	// {
	// 	$('#ItemBlockArea input').prop("checked",false);
	// }
	// else
	// {
	// 	$('#ItemBlockArea input').prop("checked",true);
	// }
}

function ShowNotice( nShow )
{
	if(nShow==0)
	{
		$("#NoticeMask").hide();
		$("#NoticeBody").hide();
	}
	else
	{
		$("#NoticeMask").show();
		$("#NoticeBody").show();
	}
}


function ResponseFilamentResult()
{
	let FilaSelectedList= $("#ItemBlockArea input:checked:visible");
	let nAll=FilaSelectedList.length;

	if( nAll==0 )
	{
		
		ShowNotice(1);
		return false;
	}
	
	let FilaArray=new Array();
	for(let n=0;n<nAll;n++)
	{
		let sName=FilaSelectedList[n].getAttribute("name");
		let sVendor = FilaSelectedList[n].getAttribute("vendor").trim();

	    for( let key in m_ProfileItem['filament'] )
	    {
			let FName=GetFilamentShortname(key);
			let oneFila = m_ProfileItem['filament'][key];
			let fVendor=oneFila['vendor'].trim();
			
			if((FName==sName)&&(fVendor==sVendor))
				FilaArray.push(key);
		}
	}
	
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="save_userguide_filaments";
	tSend['data']={};
	tSend['data']['filament']=FilaArray;
	
	SendWXMessage( JSON.stringify(tSend) );
	
	return true;
}


function CancelSelect()
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="user_guide_cancel";
	tSend['data']={};
		
	SendWXMessage( JSON.stringify(tSend) );			
}


function ConfirmSelect()
{
	let bRet=ResponseFilamentResult();
	
	if(bRet)
    {
		var tSend={};
		tSend['sequence_id']=Math.round(new Date() / 1000);
		tSend['command']="user_guide_finish";
		tSend['data']={};
		tSend['data']['action']="finish";
		
		SendWXMessage( JSON.stringify(tSend) );			
	}
}

function changeTheme () {
	$(".CValues").css({ background: '#fff' , border: '1px solid #333' })
	$('#SystemFilamentBtn').css({ color: '#333'});
	$('#CustomFilamentBtn').css({ color: '#333'});
	$('.filaTypeItem').css({ background: '#fff' })
	$('.filaType').css({ background: '#fff' })
	$('#AcceptBtn').hover(function () {
		$(this).css({ background: '#fff' , color: '#333', border: '1px solid #333'})
	}, function () {
		$(this).css({ background: '#17CC5F' , color: '#fff', border: '1px solid transparent'})
	})
}

function OnSelectMenu( nIndex )
{
	switch(nIndex)
	{
		case 1:
			$('#SystemFilamentBtn').addClass('TitleSelected');
			$('#SystemFilamentBtn').removeClass('TitleUnselected');		
			
			$('#CustomFilamentBtn').addClass('TitleUnselected');
			$('#CustomFilamentBtn').removeClass('TitleSelected');	
			
			$('#SystemFilamentsArea').css('display','flex');
			$('#CustomFilamentsArea').css('display','none');
			break;
		case 2:
			$('#CustomFilamentBtn').addClass('TitleSelected');
			$('#CustomFilamentBtn').removeClass('TitleUnselected');
			
			$('#SystemFilamentBtn').addClass('TitleUnselected');
			$('#SystemFilamentBtn').removeClass('TitleSelected');	
			
			$('#CustomFilamentsArea').css('display','flex');
			$('#SystemFilamentsArea').css('display','none');			
			break;
	}
}

function RequestCustomFilaments()
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="request_custom_filaments";
		
	SendWXMessage( JSON.stringify(tSend) );		
}

function TestCustomFilaments()
{
	let strTest='{"command":"update_custom_filaments","data":[{"id":"P0c71f94","name":"AMOLEN ABS 222"},{"id":"P19cc6c5","name":"PrimaSelect PLA 231654"},{"id":"P93a5c3b","name":"3DJAKE PLA 111"}],"sequence_id":"2000"}';
	let tItem=JSON.parse(strTest);
	
	handleStudioCmd(tItem);
}

function UpdateCustomFilaments( CFList )
{
	let strHtml='';
	let nTotal=CFList.length;
	
	for(let n=0;n<nTotal;n++)
	{
		let pItem=CFList[n];
		
		let F_id=pItem['id'];
		let F_name=pItem['name'];
		
		let strAdd='<div class="CFilament_Item">'+
			       '<a  class="CFilament_Name" title="'+F_name+'">'+F_name+'</a><img onClick="CFEdit(\''+F_id+'\')" class="CFilament_EditBtn" src="../../image/edit.svg" />'+
		           '</div>';
		
		strHtml+=strAdd;
	}
	
	$('#CFilament_List').html(strHtml);
}


function OnClickCustomFilamentAdd()
{
	
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="create_custom_filament";
		
	SendWXMessage( JSON.stringify(tSend) );		
}

//编辑某一个自定义材料
function CFEdit( fid )
{
	//alert(fid);
	
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="modify_custom_filament";
	tSend['id']=fid;
		
	SendWXMessage( JSON.stringify(tSend) );	
}


